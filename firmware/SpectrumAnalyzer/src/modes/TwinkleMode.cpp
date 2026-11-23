#include "TwinkleMode.h"

namespace {
constexpr float kMaxStepSeconds = 0.1f;   // don't jump ahead after a stall
}  // namespace

float TwinkleMode::random01() {
  // xorshift32: fast, good enough for visual randomness, repeatable in tests.
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return static_cast<float>(rng_ >> 8) / static_cast<float>(1u << 24);
}

void TwinkleMode::reset(uint32_t nowMillis) {
  rng_ = seed_;
  lastMillis_ = nowMillis;
  for (int i = 0; i < kMaxLeds; ++i) {
    level_[i] = 0.0f;
    phase_[i] = Phase::Dark;
  }
}

void TwinkleMode::render(const RenderContext& ctx, Rgb* out, int count) {
  if (count > kMaxLeds) count = kMaxLeds;
  float dt = (ctx.nowMillis - lastMillis_) / 1000.0f;
  if (dt > kMaxStepSeconds) dt = kMaxStepSeconds;
  lastMillis_ = ctx.nowMillis;

  const float startChance = config::kTwinkleStartsPerLedPerSecond * dt;
  const float riseStep = dt / config::kTwinkleFadeInSeconds;
  const float fallStep = dt / config::kTwinkleFadeOutSeconds;

  for (int i = 0; i < count; ++i) {
    switch (phase_[i]) {
      case Phase::Dark:
        if (random01() < startChance) phase_[i] = Phase::Rising;
        break;
      case Phase::Rising:
        level_[i] += riseStep;
        if (level_[i] >= 1.0f) {
          level_[i] = 1.0f;
          phase_[i] = Phase::Falling;
        }
        break;
      case Phase::Falling:
        level_[i] -= fallStep;
        if (level_[i] <= 0.0f) {
          level_[i] = 0.0f;
          phase_[i] = Phase::Dark;
        }
        break;
    }
    // Squaring the level makes the fade look even to the eye.
    out[i] = scale(ctx.settings.colourA, level_[i] * level_[i]);
  }
}
