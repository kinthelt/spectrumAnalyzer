// spectrumAnalyzer firmware: entry point.
//
// This file only wires the parts together. The parts live under src/:
//   src/config/    hardware and tuning constants
//   src/audio/     microphone input and analysis (own task on core 0)
//   src/settings/  current settings, presets, saving to flash
//   src/modes/     lighting and spectrum modes, and the manager that picks one
//   src/leds/      LED output (FastLED)
//   src/net/       Wi-Fi, setup network, spectrum.local, status page
//   src/diag/      statistics, live audio view and the serial console
//   src/util/      logging, colours, boot information
//
// See docs/DESIGN.md for the full design and build order.

#include <Arduino.h>

#include "src/audio/AudioInput.h"
#include "src/config/Config.h"
#include "src/diag/AudioMonitor.h"
#include "src/diag/FrameStats.h"
#include "src/diag/SerialConsole.h"
#include "src/leds/LedOutput.h"
#include "src/modes/ModeManager.h"
#include "src/net/Network.h"
#include "src/net/PowerCycleCounter.h"
#include "src/settings/NvsSettingsStore.h"
#include "src/settings/SettingsManager.h"
#include "src/util/Log.h"
#include "src/util/SystemInfo.h"

static NvsSettingsStore store;
static SettingsManager settings(store);
static AudioInput audio;
static AudioMonitor audioMonitor;
static Network network;
static PowerCycleCounter powerCycles(store);
static SerialConsole console(settings, audio, audioMonitor, network);
static ModeManager modes;
static LedOutput leds;
static FrameStats frameStats(config::kStatsIntervalMillis);

static AudioFrame audioFrame;
static uint32_t lastAudioMillis = 0;
static bool haveAudio = false;
static Rgb frame[config::kNumLeds];
static uint32_t lastFrameMicros = 0;
static uint32_t appliedRevision = 0;

// Text of the placeholder status page at http://spectrum.local (until step 7).
static String describeStatus() {
  const LightSettings& s = settings.current();
  char colourA[7];
  formatHexColour(s.colourA, colourA);
  String text = "Spectrum Analyzer ";
  text += config::kFirmwareVersion;
  text += "\n\nStrip: ";
  text += s.on ? "on" : "off";
  text += "\nMode: ";
  text += modeName(s.mode);
  text += "\nBrightness: " + String(s.brightness) + " of 255";
  text += "\nColour A: #" + String(colourA);
  text += "\nAudio: ";
  text += audioFrame.silent ? "silent" : "sound";
  text += "\nWi-Fi: " + network.summary();
  text += "\nUp for " + String(millis() / 1000) + " s";
  text += "\n\nThe full web page arrives in build step 7.\n";
  return text;
}

void setup() {
  Serial.begin(config::kSerialBaud);
  delay(500);   // give the USB serial port a moment so the first lines aren't lost

  logBootInfo();
  store.begin();
  settings.begin();
  leds.begin();
  audio.begin();
  frameStats.begin(millis());

  const bool forceSetup = powerCycles.begin();
  network.setStatusDescriber(describeStatus);
  network.begin(forceSetup);

  console.printStatus();
  LOG_INFO("boot", "type 'help' in Serial Monitor for commands");
}

void loop() {
  console.poll();
  settings.tick(millis());
  network.loop(millis());
  powerCycles.tick(millis());

  // Audio frames arrive ~94 times a second from core 0; take the newest.
  if (audio.latest(&audioFrame)) {
    haveAudio = true;
    lastAudioMillis = millis();
    frameStats.recordAudio(audioFrame.dspMicros);
    audioMonitor.onFrame(audioFrame, millis());
  }

  const uint32_t nowMicros = micros();
  if (nowMicros - lastFrameMicros < config::kFrameIntervalMicros) return;
  lastFrameMicros = nowMicros;

  const LightSettings& s = settings.current();
  if (settings.revision() != appliedRevision) {
    leds.setBrightness(s.brightness);
    appliedRevision = settings.revision();
  }

  const uint32_t nowMillis = millis();
  // Audio that has stopped arriving counts as none (the spectrum modes go dark).
  const bool audioFresh = haveAudio && nowMillis - lastAudioMillis < config::kAudioStaleMillis;
  modes.render(s, audioFresh ? &audioFrame : nullptr, nowMillis, frame, config::kNumLeds);
  leds.write(frame, config::kNumLeds);
  leds.show();

  frameStats.recordFrame(leds.lastShowMicros());
  frameStats.logIfDue(nowMillis);
}
