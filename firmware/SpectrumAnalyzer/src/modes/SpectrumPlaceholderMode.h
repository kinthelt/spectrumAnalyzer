// Stand-in for the two spectrum modes until the audio pipeline exists (build
// steps 4-5): a comet bouncing between the ends, coloured the way the spectrum
// will be (rainbow cycle or colour A), so the colouring can be checked now.
#pragma once

#include "LightMode.h"

class SpectrumPlaceholderMode : public LightMode {
 public:
  void reset(uint32_t nowMillis) override { startMillis_ = nowMillis; }
  void render(const RenderContext& ctx, Rgb* out, int count) override;

 private:
  uint32_t startMillis_ = 0;
};
