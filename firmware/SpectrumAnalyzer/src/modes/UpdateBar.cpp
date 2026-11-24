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
    case UpdateProgress::Phase::Receiving:
      fill(out, count, kReceiving);
      return true;
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
