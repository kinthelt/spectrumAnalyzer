// Every LED in colour A.
#pragma once

#include "LightMode.h"

class SolidMode : public LightMode {
 public:
  void render(const RenderContext& ctx, Rgb* out, int count) override {
    for (int i = 0; i < count; ++i) out[i] = ctx.settings.colourA;
  }
};
