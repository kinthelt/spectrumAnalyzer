#include "ModeManager.h"

LightMode& ModeManager::modeFor(Mode mode) {
  switch (mode) {
    case Mode::Solid: return solid_;
    case Mode::Gradient: return gradient_;
    case Mode::Rainbow: return rainbow_;
    case Mode::Twinkle: return twinkle_;
    case Mode::SpectrumSnappy: return spectrumSnappy_;
    case Mode::SpectrumSmooth:
    default: return spectrumSmooth_;
  }
}

bool ModeManager::allocate(int count) {
  for (LightMode* m : {static_cast<LightMode*>(&solid_), static_cast<LightMode*>(&gradient_),
                       static_cast<LightMode*>(&rainbow_), static_cast<LightMode*>(&twinkle_),
                       static_cast<LightMode*>(&spectrumSnappy_), static_cast<LightMode*>(&spectrumSmooth_)}) {
    if (!m->allocate(count)) return false;
  }
  started_ = false;   // reset the active mode on the next frame
  return true;
}

void ModeManager::render(const LightSettings& settings, const AudioFrame* audio, const float* levels,
                         uint32_t nowMillis, Rgb* out, int count) {
  LightMode& mode = modeFor(settings.mode);
  if (!started_ || settings.mode != active_) {
    mode.reset(nowMillis);
    active_ = settings.mode;
    started_ = true;
  }

  if (!settings.on) {
    for (int i = 0; i < count; ++i) out[i] = Rgb{};
    return;
  }
  mode.render(RenderContext{nowMillis, settings, audio, audio != nullptr ? levels : nullptr}, out, count);
}
