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
#include "src/diag/SerialConsole.h"
#include "src/leds/LedTask.h"
#include "src/net/Connectivity.h"
#include "src/net/PowerCycleCounter.h"
#include "src/settings/NvsSettingsStore.h"
#include "src/settings/SettingsManager.h"
#include "src/util/Log.h"
#include "src/util/Shared.h"
#include "src/util/SystemInfo.h"

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
static LedTask ledTask(audio, sharedSettings);
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

// Text of the placeholder status page at http://spectrum.local (until step 7).
static String describeStatus() {
  const LightSettings& s = settings.current();
  AudioFrame audioNow;
  const bool haveAudio = audio.snapshot(&audioNow);
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
  text += !haveAudio ? "none" : (audioNow.silent ? "silent" : "sound");
  text += "\nWi-Fi: " + connectivity.summary();
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
  publishSettings();
  audio.begin();
  ledTask.start();

  const bool forceSetup = powerCycles.begin();
  connectivity.setStatusDescriber(describeStatus);
  connectivity.begin(forceSetup);

  console.printStatus();
  LOG_INFO("boot", "type 'help' in Serial Monitor for commands");
}

void loop() {
  console.poll();
  settings.tick(millis());
  publishSettings();
  connectivity.loop(millis());
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
