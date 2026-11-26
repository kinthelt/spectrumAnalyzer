// spectrumAnalyzer firmware: entry point.
//
// This file only wires the parts together. The parts live under src/:
//   src/config/    hardware and tuning constants
//   src/audio/     microphone input and analysis (own task on core 0)
//   src/settings/  current settings, presets, saving to flash
//   src/modes/     lighting and spectrum modes, and the manager that picks one
//   src/leds/      LED output (FastLED)
//   src/net/       Wi-Fi, setup network, spectrum.local
//   src/web/       the web page and its API
//   src/update/    firmware updates over Wi-Fi, and going back if one fails
//   src/smarthome/ Apple Home and Google Home, through Matter
//   src/diag/      statistics, live audio view and the serial console
//   src/util/      logging, colours, boot information
//
// See docs/DESIGN.md for the full design and build order.

#include <Arduino.h>
#include <WiFi.h>

#include "src/audio/AudioInput.h"
#include "src/config/Config.h"
#include "src/diag/AudioMonitor.h"
#include "src/diag/SerialConsole.h"
#include "src/leds/LedTask.h"
#include "src/net/Connectivity.h"
#include "src/net/PowerCycleCounter.h"
#include "src/net/StationIp.h"
#include "src/settings/NvsSettingsStore.h"
#include "src/settings/SettingsManager.h"
#include "src/settings/StripSetupManager.h"
#include "src/smarthome/MatterBridge.h"
#include "src/update/FirmwareUpdate.h"
#include "src/util/Log.h"
#include "src/util/Shared.h"
#include "src/util/SystemInfo.h"
#include "src/web/WebUi.h"

// Who runs where:
//   core 0  audio task (AudioInput)               mic, filter bank, levels
//   core 1  LED task (LedTask), priority 3        modes, FastLED, every 10 ms
//   core 1  Arduino loop(), priority 1            settings, console, Wi-Fi, web, Home sync
//   Matter's own tasks                            Apple/Google Home; changes are posted to the loop
// The loop is the only writer of settings; it publishes a copy for the LED task.

static NvsSettingsStore store;
static SettingsManager settings(store);
static StripSetupManager strip(store);
static Shared<LightSettings> sharedSettings;
static uint32_t publishedRevision = 0;
static AudioInput audio;
static FirmwareUpdate firmwareUpdate;
static MatterBridge smartHome(settings);
static LedTask ledTask(audio, sharedSettings, firmwareUpdate.progress());
static AudioMonitor audioMonitor;
static Connectivity connectivity;
static PowerCycleCounter powerCycles(store);
static SerialConsole console(settings, audio, audioMonitor, connectivity);
static uint32_t monitoredSequence = 0;

// Gives the LED task a copy of the settings whenever they change.
static void publishSettings() {
  if (settings.revision() == publishedRevision) return;
  sharedSettings.set(settings.current());
  publishedRevision = settings.revision();
}

// Starts the audio and the LEDs once Matter's start-up memory spike is over
// (kMatterSettleMillis in Config.h): they need ~46 KB of the same internal RAM.
static bool lightsStarted = false;
static bool audioStarted = false;

static void startLightsWhenReady() {
  if (lightsStarted) return;
  const char* why = nullptr;
  if (smartHome.started()) {
    if (!smartHome.settled()) return;   // never during the spike, however long we've waited
    why = "after Matter's start-up";
  } else if (smartHome.status().paused) {
    why = "Apple/Google Home is paused";
  } else if (millis() >= config::kLightsStartLatestMillis) {
    why = "no Wi-Fi yet, so not waiting for Matter";
  } else {
    return;
  }
  lightsStarted = true;
  LOG_INFO("boot", "starting the audio and LEDs (%s)", why);
  const StripSetup& setup = strip.running();
  audioStarted = audio.begin(setup.ledCount);
  logMemory("audio started");
  ledTask.start(setup.ledCount);
  logMemory("LEDs started");
}

// Facts for the web page's status panel.
static DeviceStatus deviceStatus();
static void restartNow() {
  settings.flush();
  LOG_INFO("boot", "restarting");
  delay(100);   // let the log line out
  ESP.restart();
}

static WebUi webUi(settings, strip, firmwareUpdate, WebUi::Hooks{
                                 deviceStatus,
                                 [] { audio.requestNoiseLearning(); },
                                 [] { connectivity.forgetWifi(); },
                                 [] { return smartHome.openPairing(); },
                                 [] { return smartHome.removeAllHomes(); },
                                 [] { smartHome.resume(); },
                                 [] { ledTask.setMirrored(strip.running().bassOnRight); },
                                 restartNow,
                             });

static DeviceStatus deviceStatus() {
  DeviceStatus st;
  st.version = config::kFirmwareVersion;
  st.hostname = config::kHostname;
  st.uptimeSeconds = millis() / 1000;
  st.updateMaxBytes = firmwareUpdate.maxBytes();
  st.updateOnTrial = firmwareUpdate.onTrial();
  st.updateLastUndone = firmwareUpdate.lastUpdateUndone();

  const SmartHomeStatus& home = smartHome.status();
  st.homeStarted = home.started;
  st.homePaused = home.paused;
  st.homeApps = home.homes;
  st.homePairingOpen = home.pairingOpen;
  st.homeManualCode = home.manualCode;
  st.homeQrPayload = home.qrPayload;

  st.stripChosen = strip.chosen();
  st.stripRunning = strip.running();
  st.heapFree = ESP.getFreeHeap();
  st.heapLowest = ESP.getMinFreeHeap();

  uint32_t ip = 0;
  if (connectivity.connected() && stationIp(&ip)) {
    st.wifiConnected = true;
    st.ssid = WiFi.SSID().c_str();
    st.rssi = WiFi.RSSI();
    st.ip = ipToString(ip).c_str();
  }

  // Audio is "ok" while new frames keep arriving (about 94 a second).
  static uint32_t lastSequence = 0;
  static uint32_t lastChangeMillis = 0;
  AudioFrame frame;
  if (audioStarted && audio.snapshot(&frame)) {
    if (frame.sequence != lastSequence) {
      lastSequence = frame.sequence;
      lastChangeMillis = millis();
    }
    st.audioOk = millis() - lastChangeMillis < 1000;
    st.silent = frame.silent;
    st.learningNoise = frame.learningNoise;
    // Share of core 0 used by the analysis: time per block / time between blocks.
    const uint32_t blockMicros =
        static_cast<uint32_t>(1000000ULL * config::kAudioBlockSize / config::kSampleRate);
    st.audioLoadPercent = static_cast<int32_t>((frame.dspMicros * 100 + blockMicros / 2) / blockMicros);
  }
  return st;
}

void setup() {
  Serial.begin(config::kSerialBaud);
  delay(500);   // give the USB serial port a moment so the first lines aren't lost

  logBootInfo();
  logMemory("start");
  store.begin();
  settings.begin();
  publishSettings();
  firmwareUpdate.begin(millis());
  firmwareUpdate.setBeforeRestart([] { settings.flush(); });
  firmwareUpdate.setPauseHook([](bool paused) { audio.setPaused(paused); });
  smartHome.begin(resetWasCrash());

  strip.begin();
  ledTask.setMirrored(strip.running().bassOnRight);
  // The audio and LEDs start from loop(), after Matter's start-up (see startLightsWhenReady).

  const bool forceSetup = powerCycles.begin(resetWasPowerUp());
  connectivity.setWebService(&webUi);
  connectivity.begin(forceSetup);
  logMemory("Wi-Fi started");

  console.printStatus();
  LOG_INFO("boot", "type 'help' in Serial Monitor for commands");
}

void loop() {
  console.poll();
  settings.tick(millis());
  publishSettings();
  connectivity.loop(millis());
  // New firmware counts as good once it is on the Wi-Fi with the web page running.
  firmwareUpdate.loop(connectivity.connected() && webUi.running(), millis());
  // Apple Home / Google Home: starts with the first Wi-Fi connection.
  smartHome.loop(connectivity.connected(), millis());
  startLightsWhenReady();
  powerCycles.tick(millis());

  // Live audio view: read the snapshot (the LED task consumes the queue).
  AudioFrame audioNow;
  if (audioMonitor.enabled() && audio.snapshot(&audioNow) &&
      audioNow.sequence != monitoredSequence) {
    monitoredSequence = audioNow.sequence;
    audioMonitor.onFrame(audioNow, audio.bandCount(), millis());
  }

  // How much of each task's stack has never been used, for sizing them (memory is tight).
  static uint32_t lastStackLogMillis = 0;
  if (millis() - lastStackLogMillis >= 30000) {
    lastStackLogMillis = millis();
    LOG_INFO("memory", "stack never used: audio %lu of %lu B, LEDs %lu of %lu B, loop %lu B",
             (unsigned long)audio.stackSpareBytes(), (unsigned long)config::kAudioTaskStackBytes,
             (unsigned long)ledTask.stackSpareBytes(), (unsigned long)config::kLedTaskStackBytes,
             (unsigned long)uxTaskGetStackHighWaterMark(nullptr));
  }

  delay(1);   // let lower-priority tasks run; nothing here needs sub-millisecond timing
}
