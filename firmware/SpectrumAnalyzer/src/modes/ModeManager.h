// Picks the mode named in the settings and renders it. Resets a mode when it
// becomes active, and outputs black while the strip is switched off.
#pragma once

#include "GradientMode.h"
#include "LightMode.h"
#include "RainbowMode.h"
#include "SolidMode.h"
#include "SpectrumMode.h"
#include "TwinkleMode.h"

class ModeManager {
 public:
  // Sets up every mode's per-LED state for count LEDs. False if out of memory.
  bool allocate(int count);

  // audio: the latest analysis, or nullptr if none has arrived recently;
  // levels: its level per LED (count of them), nullptr with audio.
  void render(const LightSettings& settings, const AudioFrame* audio, const float* levels,
              uint32_t nowMillis, Rgb* out, int count);

  // The mode rendered last frame (useful for logging).
  Mode activeMode() const { return active_; }

 private:
  LightMode& modeFor(Mode mode);

  SolidMode solid_;
  GradientMode gradient_;
  RainbowMode rainbow_;
  TwinkleMode twinkle_;
  SpectrumMode spectrumSnappy_{SpectrumMode::Style::Snappy};
  SpectrumMode spectrumSmooth_{SpectrumMode::Style::Smooth};

  Mode active_ = Mode::Solid;
  bool started_ = false;
};
