#include "Rgb.h"

#include <cmath>

namespace {

float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

uint8_t toByte(float x) { return static_cast<uint8_t>(clamp01(x) * 255.0f + 0.5f); }

}  // namespace

Rgb hueToRgb(float hue) {
  hue -= std::floor(hue);                // wrap into 0..1
  const float h6 = hue * 6.0f;
  const int sector = static_cast<int>(h6) % 6;
  const float f = h6 - std::floor(h6);   // position within the sector
  switch (sector) {
    case 0: return Rgb{255, toByte(f), 0};          // red -> yellow
    case 1: return Rgb{toByte(1 - f), 255, 0};      // yellow -> green
    case 2: return Rgb{0, 255, toByte(f)};          // green -> cyan
    case 3: return Rgb{0, toByte(1 - f), 255};      // cyan -> blue
    case 4: return Rgb{toByte(f), 0, 255};          // blue -> magenta
    default: return Rgb{255, 0, toByte(1 - f)};     // magenta -> red
  }
}

Rgb lerp(const Rgb& a, const Rgb& b, float t) {
  t = clamp01(t);
  auto mix = [t](uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(x + (static_cast<float>(y) - x) * t + 0.5f);
  };
  return Rgb{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

Rgb scale(const Rgb& c, float level) {
  level = clamp01(level);
  return Rgb{static_cast<uint8_t>(c.r * level + 0.5f), static_cast<uint8_t>(c.g * level + 0.5f),
             static_cast<uint8_t>(c.b * level + 0.5f)};
}
