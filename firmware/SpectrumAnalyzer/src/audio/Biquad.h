// Second-order IIR filter section (transposed direct form II), with the RBJ
// "Audio EQ Cookbook" band-pass and low-pass designs. Plain C++.
#pragma once

#include <cmath>

struct Biquad {
  float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  float z1 = 0, z2 = 0;

  inline float process(float x) {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }

  void resetState() { z1 = z2 = 0; }

  // Band-pass with 0 dB gain at the centre frequency f0.
  void setBandPass(float fs, float f0, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * f0 / fs;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = -2.0f * std::cos(w0) / a0;
    a2 = (1.0f - alpha) / a0;
  }

  void setLowPass(float fs, float f0, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * f0 / fs;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float c = std::cos(w0);
    const float a0 = 1.0f + alpha;
    b0 = (1.0f - c) / 2.0f / a0;
    b1 = (1.0f - c) / a0;
    b2 = b0;
    a1 = -2.0f * c / a0;
    a2 = (1.0f - alpha) / a0;
  }
};
