// spectrumAnalyzer firmware: entry point.
//
// This file only wires the parts together. The parts live under src/:
//   src/config/  hardware and tuning constants
//   src/leds/    LED output (FastLED)
//   src/diag/    bench test pattern and frame statistics
//   src/util/    logging and boot information
//
// See docs/DESIGN.md for the full design and build order.

#include <Arduino.h>

#include "src/config/Config.h"
#include "src/diag/BenchTest.h"
#include "src/diag/FrameStats.h"
#include "src/leds/LedOutput.h"
#include "src/util/SystemInfo.h"

static LedOutput leds;
static BenchTest benchTest;
static FrameStats frameStats(config::kStatsIntervalMillis);
static uint32_t lastFrameMicros = 0;

void setup() {
  Serial.begin(config::kSerialBaud);
  delay(500);   // give the USB serial port a moment so the first lines aren't lost

  logBootInfo();
  leds.begin();
  benchTest.start(millis());
  frameStats.begin(millis());
}

void loop() {
  const uint32_t nowMicros = micros();
  if (nowMicros - lastFrameMicros < config::kFrameIntervalMicros) return;
  lastFrameMicros = nowMicros;

  const uint32_t nowMillis = millis();
  benchTest.render(leds.pixels(), LedOutput::kSize, nowMillis);
  leds.show();

  frameStats.recordFrame(leds.lastShowMicros());
  frameStats.logIfDue(nowMillis);
}
