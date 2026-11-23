// spectrumAnalyzer firmware: entry point.
//
// This file only wires the parts together. The parts live under src/:
//   src/config/    hardware and tuning constants
//   src/audio/     microphone input and analysis (own task on core 0)
//   src/settings/  current settings, presets, saving to flash
//   src/modes/     lighting and spectrum modes, and the manager that picks one
//   src/leds/      LED output (FastLED)
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
#include "src/settings/NvsSettingsStore.h"
#include "src/settings/SettingsManager.h"
#include "src/util/Log.h"
#include "src/util/SystemInfo.h"

static NvsSettingsStore store;
static SettingsManager settings(store);
static AudioInput audio;
static AudioMonitor audioMonitor;
static SerialConsole console(settings, audio, audioMonitor);
static ModeManager modes;
static LedOutput leds;
static FrameStats frameStats(config::kStatsIntervalMillis);

static AudioFrame audioFrame;
static Rgb frame[config::kNumLeds];
static uint32_t lastFrameMicros = 0;
static uint32_t appliedRevision = 0;

void setup() {
  Serial.begin(config::kSerialBaud);
  delay(500);   // give the USB serial port a moment so the first lines aren't lost

  logBootInfo();
  store.begin();
  settings.begin();
  leds.begin();
  audio.begin();
  frameStats.begin(millis());

  console.printStatus();
  LOG_INFO("boot", "type 'help' in Serial Monitor for commands");
}

void loop() {
  console.poll();
  settings.tick(millis());

  // Audio frames arrive ~94 times a second from core 0; take the newest.
  if (audio.latest(&audioFrame)) {
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
  modes.render(s, nowMillis, frame, config::kNumLeds);
  leds.write(frame, config::kNumLeds);
  leds.show();

  frameStats.recordFrame(leds.lastShowMicros());
  frameStats.logIfDue(nowMillis);
}
