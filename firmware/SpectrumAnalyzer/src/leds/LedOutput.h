// Owns the pixel buffer and sends it to the strip through FastLED.
// Nothing else in the firmware talks to FastLED's global object directly.
#pragma once

#include <FastLED.h>

#include "../config/Config.h"
#include "../util/Rgb.h"

class LedOutput {
 public:
  static constexpr int kSize = config::kNumLeds;

  // Registers the strip with FastLED, applies the power limit and clears the strip.
  void begin();

  // Copies a frame rendered by the modes into the output buffer. Call show() after.
  void write(const Rgb* frame, int count);

  void setBrightness(uint8_t brightness);
  uint8_t brightness() const { return brightness_; }

  // Sends the current frame to the strip and records how long that took.
  void show();

  // Turns every LED off immediately.
  void clear();

  // Duration of the most recent show(), in microseconds.
  uint32_t lastShowMicros() const { return lastShowMicros_; }

 private:
  CRGB leds_[kSize];
  uint8_t brightness_ = config::kDefaultBrightness;
  uint32_t lastShowMicros_ = 0;
};
