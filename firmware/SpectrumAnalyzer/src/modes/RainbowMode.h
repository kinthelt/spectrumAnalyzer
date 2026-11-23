// The whole strip in one colour that slowly travels round the colour wheel.
#pragma once

#include "../config/Config.h"
#include "LightMode.h"

// Hue of the slow rainbow cycle at a given time. Shared with the spectrum modes.
inline float rainbowCycleHue(uint32_t nowMillis) {
  return static_cast<float>(nowMillis % config::kRainbowCycleMillis) / config::kRainbowCycleMillis;
}

class RainbowMode : public LightMode {
 public:
  void render(const RenderContext& ctx, Rgb* out, int count) override {
    const Rgb colour = hueToRgb(rainbowCycleHue(ctx.nowMillis));
    for (int i = 0; i < count; ++i) out[i] = colour;
  }
};
