#include "BenchTest.h"

namespace {
constexpr uint32_t kColourStepMillis = 1000;
constexpr uint32_t kColourCheckMillis = 3 * kColourStepMillis;
constexpr uint32_t kCometStepMillis = 15;   // ~2.2 s from one end to the other
constexpr int kCometTail = 12;              // LEDs in the fading tail
}  // namespace

void BenchTest::render(CRGB* pixels, int count, uint32_t nowMillis, const CRGB& colour) {
  const uint32_t elapsed = nowMillis - startMillis_;
  fill_solid(pixels, count, CRGB::Black);

  // 1. Colour-order check on the first LED.
  if (elapsed < kColourCheckMillis) {
    static const CRGB kSteps[] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    pixels[0] = kSteps[elapsed / kColourStepMillis];
    return;
  }

  // 2. Comet bouncing between the ends. One full trip is 2 * (count - 1) steps.
  const uint32_t step = (elapsed - kColourCheckMillis) / kCometStepMillis;
  const uint32_t period = 2u * static_cast<uint32_t>(count - 1);
  const uint32_t phase = step % period;
  const bool forward = phase < static_cast<uint32_t>(count - 1);
  const int head = forward ? static_cast<int>(phase) : static_cast<int>(period - phase);
  const int direction = forward ? 1 : -1;

  for (int t = 0; t <= kCometTail; ++t) {
    const int i = head - direction * t;
    if (i < 0 || i >= count) break;
    const uint16_t level = 255 - (255 * t) / (kCometTail + 1);   // 255 at the head
    pixels[i] = CRGB(static_cast<uint8_t>(colour.r * level / 255),
                     static_cast<uint8_t>(colour.g * level / 255),
                     static_cast<uint8_t>(colour.b * level / 255));
  }
}
