// Interface every mode implements. A mode turns the settings (and, for the
// spectrum modes, the latest audio analysis) into one frame of colours.
// Plain C++ so modes are unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../audio/AudioFrame.h"
#include "../settings/LightSettings.h"
#include "../util/Rgb.h"

struct RenderContext {
  uint32_t nowMillis;
  const LightSettings& settings;
  const AudioFrame* audio;   // latest analysis; nullptr if none has arrived recently
  const float* levels;       // its level per LED (one per band); nullptr with audio
};

class LightMode {
 public:
  virtual ~LightMode() = default;

  // Sets up per-LED state for count LEDs (called at start-up). False if out of memory.
  virtual bool allocate(int count) {
    (void)count;
    return true;
  }

  // Called when the mode becomes active, so animations start from a clean state.
  virtual void reset(uint32_t nowMillis) { (void)nowMillis; }

  // Writes count colours into out. Called once per frame (100 times a second).
  virtual void render(const RenderContext& ctx, Rgb* out, int count) = 0;
};
