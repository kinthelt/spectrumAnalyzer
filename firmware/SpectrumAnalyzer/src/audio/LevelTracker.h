// Turns per-band power into a 0..1 level per LED.
//
//   power -> dB + treble tilt -> per-band noise floor -> automatic gain -> 0..1
//
// Noise floors are learned without a calibration step: the overall level is
// tracked to spot quiet moments, and only then do the floors settle on the
// current band levels. While there is sound they can only fall. The quiet-level
// estimate itself only rises while the overall level is steady, which noise is
// and music isn't. So music can't drag the floors up, while a new steady noise
// (a fan starting, a hum) is learned within about half a minute.
//
// Each band also learns how much its noise varies, and its gate sits a few
// standard deviations above the average, so noise doesn't flicker the LEDs.
//
// Plain C++ so it is unit-tested on a PC.
#pragma once

#include "../config/Config.h"
#include "AudioFrame.h"

class LevelTracker {
 public:
  static constexpr int kBands = config::kNumLeds;

  // centreHz: each band's centre frequency (for the treble tilt).
  void begin(const float* centreHz);

  // bandPower: mean-square power per band. dtSeconds: time since the last call.
  // Fills everything in out except sequence and dspMicros.
  void update(const float* bandPower, float dtSeconds, AudioFrame* out);

  float floorDb(int band) const { return floorDb_[band]; }
  float gateDb(int band) const;   // floor plus the band's noise margin

 private:
  float tiltDb_[kBands] = {};
  float floorDb_[kBands] = {};
  float noiseVarDb2_[kBands] = {};   // variance of the band's level around its floor, dB^2
  float broadFastDb_ = -120, broadSlowDb_ = -120, broadDeviationDb_ = 0;
  float quietRefDb_ = config::kQuietCeilingDb;
  float agcRefDb_ = -120;
  float silentSeconds_ = 0;
  float warmupSeconds_ = 0;   // counts up to kWarmupSeconds after begin()
};
