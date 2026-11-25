// Multirate constant-Q filter bank: one 4th-order band-pass filter per LED.
//
// An FFT has equal-width bins, so with ~17 log-spaced bands per octave the bass
// LEDs would share bins. Here every LED gets its own band-pass filter, and the
// audio is low-passed and halved in sample rate once per octave going down.
// Each band runs at the lowest rate that still comfortably holds it, so:
//   - total CPU is roughly twice the cost of the top octave, not 150 full-rate filters;
//   - every filter's centre sits between Fs/16 and Fs/8 of its stage, which keeps
//     the float coefficients well conditioned even at 40 Hz.
//
// Plain C++ so it is unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../config/Config.h"
#include "Biquad.h"

class FilterBank {
 public:
  static constexpr int kMaxBands = config::kMaxLedCount;
  static constexpr int kBlock = config::kAudioBlockSize;
  static constexpr int kMaxStages = 9;

  // Designs bands filters (1 to kMaxBands, one per LED), log-spaced from lowHz to
  // highHz; one band sits in the range's middle, as wide as the range. Returns
  // false if bands is out of range or the settings need more decimation stages
  // than the block size allows.
  bool begin(int bands, float sampleRate, float lowHz, float highHz, float qScale, float minBandwidthHz);

  int bandCount() const { return bands_; }

  // Feeds exactly kBlock samples (full scale = +/-1.0). Afterwards power(i) holds
  // the smoothed mean-square output of band i (a full-scale sine gives 0.5).
  void process(const float* in);

  float power(int band) const { return env_[band]; }
  float centreHz(int band) const { return centreHz_[band]; }
  float q(int band) const { return q_[band]; }
  int stageOf(int band) const { return stageOf_[band]; }
  int stageCount() const { return numStages_; }

  // Band-samples processed per second; each is two biquads plus an envelope update.
  double bandSamplesPerSecond() const;

 private:
  float fs_ = 48000.0f;
  int bands_ = 0;
  int numStages_ = 0;
  float centreHz_[kMaxBands] = {};
  float q_[kMaxBands] = {};
  uint8_t stageOf_[kMaxBands] = {};

  Biquad bandPass_[kMaxBands][2];
  float env_[kMaxBands] = {};
  float envCoef_[kMaxBands] = {};
  Biquad antiAlias_[kMaxStages][2];
  uint8_t stageBands_[kMaxStages][kMaxBands] = {};
  int stageBandCount_[kMaxStages] = {};
  float buf_[2][kBlock / 2] = {};
};
