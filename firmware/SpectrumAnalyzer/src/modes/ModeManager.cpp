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

void ModeManager::render(const LightSettings& settings, const AudioFrame* audio,
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
  mode.render(RenderContext{nowMillis, settings, audio}, out, count);
}
