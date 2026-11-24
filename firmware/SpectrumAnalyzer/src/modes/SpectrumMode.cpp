#include "SpectrumMode.h"

#include <cmath>

#include "RainbowMode.h"

namespace {
constexpr float kMaxStepSeconds = 0.1f;   // don't jump ahead after a stall
const Rgb kWhite{255, 255, 255};
}  // namespace

void SpectrumMode::reset(uint32_t nowMillis) {
  lastMillis_ = nowMillis;
  for (int i = 0; i < kMaxLeds; ++i) {
    display_[i] = 0.0f;
    flash_[i] = 0.0f;
    hold_[i] = 0.0f;
  }
  fade_ = 0.0f;
}

void SpectrumMode::updateSnappy(int i, float level, float dt) {
  float& d = display_[i];
  if (level - d >= config::kFlashJump && level >= config::kFlashMinLevel) flash_[i] = 1.0f;
  if (level >= d) {
    d = level;
  } else {
    d -= config::kSnappyFallPerSecond * dt;
    if (d < level) d = level;
  }
  flash_[i] -= dt / config::kFlashSeconds;
  if (flash_[i] < 0.0f) flash_[i] = 0.0f;
}

void SpectrumMode::updateSmooth(int i, float level, float dt) {
  float& d = display_[i];
  if (level >= d) {
    float a = dt / config::kSmoothAttackSeconds;
    if (a > 1.0f) a = 1.0f;
    d += (level - d) * a;
    hold_[i] = config::kSmoothHoldSeconds;
  } else if (hold_[i] > 0.0f) {
    hold_[i] -= dt;
  } else {
    d -= config::kSmoothFallPerSecond * dt;
    if (d < level) d = level;
  }
}

void SpectrumMode::render(const RenderContext& ctx, Rgb* out, int count) {
  if (count > kMaxLeds) count = kMaxLeds;
  float dt = (ctx.nowMillis - lastMillis_) / 1000.0f;
  if (dt > kMaxStepSeconds) dt = kMaxStepSeconds;
  lastMillis_ = ctx.nowMillis;

  const AudioFrame* audio = ctx.audio;
  const bool silent = audio == nullptr || audio->silent;

  // Fade out over kSilenceFadeSeconds in silence; back to full at once on sound.
  if (silent) {
    fade_ -= dt / config::kSilenceFadeSeconds;
    if (fade_ < 0.0f) fade_ = 0.0f;
  } else {
    fade_ = 1.0f;
  }

  const Rgb base = ctx.settings.spectrumColour == SpectrumColour::RainbowCycle
                       ? hueToRgb(rainbowCycleHue(ctx.nowMillis))
                       : ctx.settings.colourA;

  for (int i = 0; i < count; ++i) {
    const float level = audio != nullptr ? audio->level[i] : 0.0f;
    if (style_ == Style::Snappy) {
      updateSnappy(i, level, dt);
    } else {
      updateSmooth(i, level, dt);
    }

    const float brightness = std::pow(display_[i], config::kSpectrumGamma) * fade_;
    // A flash blends the colour toward white; it never makes a dark LED light.
    const Rgb colour = flash_[i] > 0.0f ? lerp(base, kWhite, flash_[i]) : base;
    out[i] = scale(colour, brightness);
  }
}
