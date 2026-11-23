// 8-bit RGB colour and the small amount of colour maths the modes need.
// Plain C++ so modes can be unit-tested on a PC.
#pragma once

#include <cstdint>

struct Rgb {
  uint8_t r = 0, g = 0, b = 0;
  bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const Rgb& o) const { return !(*this == o); }
};

// Fully saturated colour at hue 0..1 (0 = red, 1/3 = green, 2/3 = blue, wraps at 1).
Rgb hueToRgb(float hue);

// Straight-line blend: t = 0 gives a, t = 1 gives b. t is clamped to 0..1.
Rgb lerp(const Rgb& a, const Rgb& b, float t);

// Scales a colour by level 0..1 (clamped).
Rgb scale(const Rgb& c, float level);
