// The spectrum display: LED i shows frequency band i, brightness following its level.
//
//   Snappy  instant rise, fast fall; a sudden new peak flashes toward white.
//   Smooth  softened rise; each LED holds its peak briefly, then falls slowly.
//
// Colour: the whole strip in one hue slowly cycling through the rainbow, or
// colour A (settings.spectrumColour). In silence the strip fades to dark.
#pragma once

#include "../config/Config.h"
#include "LightMode.h"

class SpectrumMode : public LightMode {
 public:
  enum class Style { Snappy, Smooth };

  explicit SpectrumMode(Style style) : style_(style) {}

  void reset(uint32_t nowMillis) override;
  void render(const RenderContext& ctx, Rgb* out, int count) override;

  // Displayed level of LED i, 0..1, before gamma (for tests).
  float displayLevel(int i) const { return display_[i]; }

 private:
  static constexpr int kMaxLeds = config::kMaxLedCount;

  void updateSnappy(int i, float level, float dt);
  void updateSmooth(int i, float level, float dt);

  Style style_;
  uint32_t lastMillis_ = 0;
  float display_[kMaxLeds] = {};
  float flash_[kMaxLeds] = {};     // Snappy: 1 at a new peak, fading to 0
  float hold_[kMaxLeds] = {};      // Smooth: seconds left holding the peak
  float fade_ = 0.0f;              // 1 = full, 0 = faded out for silence
};
