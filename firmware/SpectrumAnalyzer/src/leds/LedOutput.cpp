#include "LedOutput.h"

#include "../util/Log.h"

void LedOutput::begin() {
  // WS2812B and SK6812 (both possible on the 1376 strip) use GRB colour order.
  FastLED.addLeds<WS2812B, config::kLedDataPin, GRB>(leds_, kSize);
  FastLED.setMaxPowerInVoltsAndMilliamps(config::kSupplyVolts, config::kPowerLimitMilliamps);
  FastLED.setBrightness(brightness_);
  clear();
  LOG_INFO("leds", "%d LEDs on GPIO %d, power limit %lu mA", kSize, config::kLedDataPin,
           (unsigned long)config::kPowerLimitMilliamps);
}

void LedOutput::setBrightness(uint8_t brightness) {
  brightness_ = brightness;
  FastLED.setBrightness(brightness_);
}

void LedOutput::write(const Rgb* frame, int count) {
  if (count > kSize) count = kSize;
  for (int i = 0; i < count; ++i) leds_[i] = CRGB(frame[i].r, frame[i].g, frame[i].b);
  for (int i = count; i < kSize; ++i) leds_[i] = CRGB::Black;
}

void LedOutput::show() {
  const uint32_t start = micros();
  FastLED.show();
  lastShowMicros_ = micros() - start;
}

void LedOutput::clear() {
  fill_solid(leds_, kSize, CRGB::Black);
  show();
}
