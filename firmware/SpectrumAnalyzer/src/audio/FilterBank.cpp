// The inner loops below are the hottest code in the firmware. The Arduino build
// optimises for size by default; this file asks for speed instead.
#pragma GCC optimize("O2")

#include "FilterBank.h"

#include <cmath>

bool FilterBank::begin(int bands, float sampleRate, float lowHz, float highHz, float qScale,
                       float minBandwidthHz) {
  if (bands < 1 || bands > kMaxBands) return false;
  fs_ = sampleRate;
  bands_ = bands;
  // Neighbouring bands are ratio apart. One band sits at the range's (geometric)
  // middle and is as wide as the whole range.
  const float ratio = bands > 1 ? std::pow(highHz / lowHz, 1.0f / static_cast<float>(bands - 1)) : 1.0f;
  const float firstHz = bands > 1 ? lowHz : std::sqrt(lowHz * highHz);
  // Q at which neighbouring bands meet at their -3 dB points.
  const float qTile = bands > 1 ? 1.0f / (std::sqrt(ratio) - 1.0f / std::sqrt(ratio))
                                : firstHz / (highHz - lowHz);
  // Each band is two identical biquads in series (4th order) for steeper skirts.
  // Two in series reach -3 dB where each is -1.5 dB, so each stage's Q is scaled
  // by sqrt(sqrt(2) - 1) to keep the overall bandwidth.
  const float cascadeQ = std::sqrt(std::sqrt(2.0f) - 1.0f);

  numStages_ = 1;
  for (int k = 0; k < kMaxStages; ++k) stageBandCount_[k] = 0;

  for (int i = 0; i < bands_; ++i) {
    const float f = firstHz * std::pow(ratio, static_cast<float>(i));
    centreHz_[i] = f;

    // Deepest stage k whose rate Fs/2^k still has f <= (Fs/2^k)/8.
    // Stage 0 takes everything else, up to Nyquist.
    int k = 0;
    while (k + 1 < kMaxStages && f <= sampleRate / static_cast<float>(1 << (k + 1)) / 8.0f) ++k;
    stageOf_[i] = static_cast<uint8_t>(k);
    if (k + 1 > numStages_) numStages_ = k + 1;
    stageBands_[k][stageBandCount_[k]++] = static_cast<uint8_t>(i);

    const float stageFs = sampleRate / static_cast<float>(1 << k);
    const float q = std::fmin(qTile * qScale, f / minBandwidthHz);
    q_[i] = q;
    bandPass_[i][0].setBandPass(stageFs, f, q * cascadeQ);
    bandPass_[i][0].resetState();
    bandPass_[i][1] = bandPass_[i][0];

    // Envelope follower on y^2: time constant ~1 period of the centre frequency
    // smooths the ripple at 2f. The 10 ms minimum (about one analysis block) keeps
    // the treble bands' noise estimates steady enough not to flicker in a quiet room.
    const float tau = std::fmax(1.0f / f, 0.010f);
    envCoef_[i] = 1.0f - std::exp(-1.0f / (tau * stageFs));
    env_[i] = 0.0f;
  }

  // Anti-alias filters: 4th-order Butterworth (two biquads) at Fs_k/6 before each
  // 2:1 decimation. Bands in stage k+1 lie below Fs_k/8, and only content above
  // 3*Fs_k/8 can fold onto them.
  for (int k = 0; k + 1 < numStages_; ++k) {
    const float stageFs = sampleRate / static_cast<float>(1 << k);
    antiAlias_[k][0].setLowPass(stageFs, stageFs / 6.0f, 0.5411961f);
    antiAlias_[k][1].setLowPass(stageFs, stageFs / 6.0f, 1.3065630f);
    antiAlias_[k][0].resetState();
    antiAlias_[k][1].resetState();
  }

  return numStages_ <= kMaxStages && kBlock % (1 << (numStages_ - 1)) == 0;
}

void FilterBank::process(const float* in) {
  // buf_ ping-pongs: stage k reads cur and writes the decimated signal to next.
  const float* cur = in;
  int n = kBlock;
  for (int k = 0; k < numStages_; ++k) {
    const int count = stageBandCount_[k];
    for (int j = 0; j < count; ++j) {
      const int i = stageBands_[k][j];
      Biquad f0 = bandPass_[i][0];   // local copies keep the state in registers
      Biquad f1 = bandPass_[i][1];
      float e = env_[i];
      const float c = envCoef_[i];
      for (int s = 0; s < n; ++s) {
        const float y = f1.process(f0.process(cur[s]));
        e += c * (y * y - e);
      }
      bandPass_[i][0] = f0;
      bandPass_[i][1] = f1;
      env_[i] = e;
    }

    if (k + 1 < numStages_) {
      float* next = buf_[k & 1];
      Biquad& a0 = antiAlias_[k][0];
      Biquad& a1 = antiAlias_[k][1];
      for (int s = 0; s < n; ++s) {
        const float y = a1.process(a0.process(cur[s]));
        if (s & 1) next[s >> 1] = y;
      }
      cur = next;
      n >>= 1;
    }
  }
}

double FilterBank::bandSamplesPerSecond() const {
  double total = 0;
  for (int i = 0; i < bands_; ++i) total += fs_ / static_cast<double>(1 << stageOf_[i]);
  return total;
}
