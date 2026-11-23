// Blend from colour A at the left end (LED 0) to colour B at the right end.
#pragma once

#include "LightMode.h"

class GradientMode : public LightMode {
 public:
  void render(const RenderContext& ctx, Rgb* out, int count) override {
    for (int i = 0; i < count; ++i) {
      const float t = count > 1 ? static_cast<float>(i) / (count - 1) : 0.0f;
      out[i] = lerp(ctx.settings.colourA, ctx.settings.colourB, t);
    }
  }
};
