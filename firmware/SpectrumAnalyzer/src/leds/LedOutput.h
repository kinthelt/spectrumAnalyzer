// Owns the pixel buffer and sends it to the strip through FastLED.
// Nothing else in the firmware talks to FastLED's global object directly.
#pragma once

#include <FastLED.h>

#include <atomic>

#include "../config/Config.h"
#include "../util/Memory.h"
#include "../util/Rgb.h"

class LedOutput {
 public:
  // Sizes the pixel buffer for the strip. False if out of memory.
  bool allocate(int count) {
    count_ = 0;
    if (!leds_.allocate(count)) return false;
    count_ = count;
    return true;
  }

  // Registers the strip with FastLED, applies the power limit and clears the strip.
  // Call once, after allocate().
  void begin();

  int count() const { return count_; }

  // Bass end on the right: every frame is mirrored, as if the strip were mounted
  // the other way round. Safe to call from any task; applies from the next frame.
  void setMirrored(bool mirrored) { mirrored_.store(mirrored); }

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
  Buffer<CRGB> leds_;
  int count_ = 0;
  std::atomic<bool> mirrored_{false};
  uint8_t brightness_ = config::kDefaultBrightness;
  uint32_t lastShowMicros_ = 0;
};
