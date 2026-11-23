#include "SpectrumPlaceholderMode.h"

#include "RainbowMode.h"

namespace {
constexpr uint32_t kStepMillis = 15;   // ~2.2 s from one end to the other
constexpr int kTail = 12;              // LEDs in the fading tail
}  // namespace

void SpectrumPlaceholderMode::render(const RenderContext& ctx, Rgb* out, int count) {
  for (int i = 0; i < count; ++i) out[i] = Rgb{};
  if (count < 2) return;

  const Rgb colour = ctx.settings.spectrumColour == SpectrumColour::RainbowCycle
                         ? hueToRgb(rainbowCycleHue(ctx.nowMillis))
                         : ctx.settings.colourA;

  const uint32_t step = (ctx.nowMillis - startMillis_) / kStepMillis;
  const uint32_t period = 2u * static_cast<uint32_t>(count - 1);
  const uint32_t phase = step % period;
  const bool forward = phase < static_cast<uint32_t>(count - 1);
  const int head = forward ? static_cast<int>(phase) : static_cast<int>(period - phase);
  const int direction = forward ? 1 : -1;

  for (int t = 0; t <= kTail; ++t) {
    const int i = head - direction * t;
    if (i < 0 || i >= count) break;
    out[i] = scale(colour, 1.0f - static_cast<float>(t) / (kTail + 1));
  }
}
