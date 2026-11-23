// Bench test pattern for build step 1. Replaced by the real modes in step 3.
//
// 1. Colour-order check: LED 0 shows red, then green, then blue (1 s each).
// 2. A comet with a fading tail runs from LED 0 to LED 149 and back, forever.
//
// Non-blocking: render() draws one frame from the current time and returns.
#pragma once

#include <FastLED.h>

class BenchTest {
 public:
  void start(uint32_t nowMillis) { startMillis_ = nowMillis; }

  // Draws the frame for time nowMillis into pixels[0..count).
  void render(CRGB* pixels, int count, uint32_t nowMillis);

 private:
  uint32_t startMillis_ = 0;
};
