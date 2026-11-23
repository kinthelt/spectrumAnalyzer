// Interface every mode implements. A mode turns the settings (and, for the
// spectrum modes in step 5, the audio levels) into one frame of colours.
// Plain C++ so modes are unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../settings/LightSettings.h"
#include "../util/Rgb.h"

struct RenderContext {
  uint32_t nowMillis;
  const LightSettings& settings;
};

class LightMode {
 public:
  virtual ~LightMode() = default;

  // Called when the mode becomes active, so animations start from a clean state.
  virtual void reset(uint32_t nowMillis) { (void)nowMillis; }

  // Writes count colours into out. Called once per frame (100 times a second).
  virtual void render(const RenderContext& ctx, Rgb* out, int count) = 0;
};
