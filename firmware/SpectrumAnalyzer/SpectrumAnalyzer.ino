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
#include "src/update/FirmwareUpdate.h"
#include "src/util/Log.h"
#include "src/util/Shared.h"
#include "src/util/SystemInfo.h"
#include "src/web/WebUi.h"

// Who runs where:
//   core 0  audio task (AudioInput)               mic, filter bank, levels
//   core 1  LED task (LedTask), priority 3        modes, FastLED, every 10 ms
//   core 1  Arduino loop(), priority 1            settings, console, Wi-Fi, web
// The loop is the only writer of settings; it publishes a copy for the LED task.

static NvsSettingsStore store;
static SettingsManager settings(store);
static Shared<LightSettings> sharedSettings;
static uint32_t publishedRevision = 0;
static AudioInput audio;
static FirmwareUpdate firmwareUpdate;
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

// Facts for the web page's status panel.
static DeviceStatus deviceStatus();
static WebUi webUi(settings, firmwareUpdate, WebUi::Hooks{
                                 deviceStatus,
                                 [] { audio.requestNoiseLearning(); },
                                 [] { connectivity.forgetWifi(); },
                             });
static bool audioStarted = false;

static DeviceStatus deviceStatus() {
  DeviceStatus st;
  st.version = config::kFirmwareVersion;
  st.hostname = config::kHostname;
  st.uptimeSeconds = millis() / 1000;
  st.updateMaxBytes = firmwareUpdate.maxBytes();
  st.updateOnTrial = firmwareUpdate.onTrial();
  st.updateLastUndone = firmwareUpdate.lastUpdateUndone();

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
  store.begin();
  settings.begin();
  publishSettings();
  firmwareUpdate.begin(millis());
  firmwareUpdate.setBeforeRestart([] { settings.flush(); });
  audioStarted = audio.begin();
  ledTask.start();

  const bool forceSetup = powerCycles.begin();
  connectivity.setWebService(&webUi);
  connectivity.begin(forceSetup);

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
  powerCycles.tick(millis());

  // Live audio view: read the snapshot (the LED task consumes the queue).
  AudioFrame audioNow;
  if (audioMonitor.enabled() && audio.snapshot(&audioNow) &&
      audioNow.sequence != monitoredSequence) {
    monitoredSequence = audioNow.sequence;
    audioMonitor.onFrame(audioNow, millis());
  }

  delay(1);   // let lower-priority tasks run; nothing here needs sub-millisecond timing
}
