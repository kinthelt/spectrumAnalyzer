// Random LEDs fade in and out in colour A, like stars.
#pragma once

#include "../config/Config.h"
#include "LightMode.h"

class TwinkleMode : public LightMode {
 public:
  static constexpr int kMaxLeds = config::kMaxLedCount;

  explicit TwinkleMode(uint32_t seed = 0x2545F491u) : seed_(seed), rng_(seed) {}

  void reset(uint32_t nowMillis) override;
  void render(const RenderContext& ctx, Rgb* out, int count) override;

 private:
  enum class Phase : uint8_t { Dark, Rising, Falling };

  float random01();   // uniform 0..1, from a small deterministic generator

  uint32_t seed_;
  uint32_t rng_;
  uint32_t lastMillis_ = 0;
  float level_[kMaxLeds] = {};
  Phase phase_[kMaxLeds] = {};
};
