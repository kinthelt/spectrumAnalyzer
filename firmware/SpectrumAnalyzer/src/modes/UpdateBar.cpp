#include "UpdateBar.h"

namespace update_bar {

namespace {
void fill(Rgb* out, int count, const Rgb& c) {
  for (int i = 0; i < count; ++i) out[i] = c;
}
bool flashOn(uint32_t elapsedMillis) { return (elapsedMillis / kFlashHalfPeriodMillis) % 2 == 0; }
}  // namespace

bool render(const UpdateProgress& progress, uint32_t nowMillis, uint32_t failFlashMillis, Rgb* out,
            int count) {
  const uint32_t elapsed = nowMillis - progress.phaseSinceMillis;
  switch (progress.phase) {
    case UpdateProgress::Phase::Idle:
      return false;
    case UpdateProgress::Phase::Receiving: {
      // Lit length in 1/100ths of an LED, so the bar's tip brightens smoothly.
      const uint32_t pct = progress.percent > 100 ? 100 : progress.percent;
      const uint32_t litHundredths = pct * static_cast<uint32_t>(count);
      const int full = static_cast<int>(litHundredths / 100);
      for (int i = 0; i < count; ++i) {
        if (i < full) {
          out[i] = kBar;
        } else if (i == full) {
          out[i] = lerp(kTrack, kBar, (litHundredths % 100) / 100.0f);
        } else {
          out[i] = kTrack;
        }
      }
      return true;
    }
    case UpdateProgress::Phase::Succeeded:
      fill(out, count, flashOn(elapsed) ? kGood : Rgb{});
      return true;
    case UpdateProgress::Phase::Failed:
      if (elapsed >= failFlashMillis) return false;
      fill(out, count, flashOn(elapsed) ? kBad : Rgb{});
      return true;
  }
  return false;
}

}  // namespace update_bar
