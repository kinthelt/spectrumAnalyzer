// Turns per-band power into a 0..1 level per LED.
//
//   power -> dB + treble tilt -> per-band noise floor -> automatic gain -> 0..1
//
// Noise floors are learned without a calibration step, while it is silent
// (nothing standing out for a moment), when the noise can be measured without
// bias. During sound, floors are frozen, so music is never learned as noise.
// A new noise loud enough to light bands (a fan) is learned on request with
// learnNoise(); a steady hum lighting only a band or two is learned by itself.
//
// Each band also learns how much its noise varies, and its gate sits a few
// standard deviations above the average, so noise doesn't flicker the LEDs.
//
// Plain C++ so it is unit-tested on a PC.
#pragma once

#include "../config/Config.h"
#include "../util/Memory.h"
#include "AudioFrame.h"

class LevelTracker {
 public:
  // bands: how many (one per LED); centreHz: each band's centre (for the treble
  // tilt). False if out of memory.
  bool begin(int bands, const float* centreHz);

  // bandPower: mean-square power per band. dtSeconds: time since the last call.
  // Updates levels() and fills everything in out except sequence and dspMicros.
  void update(const float* bandPower, float dtSeconds, AudioFrame* out);

  int bandCount() const { return bands_; }
  // Per band, 0 (at or below the noise floor / bottom of the range) to 1 (as loud
  // as the loudest band). No smoothing beyond the filters: the spectrum modes add
  // their own attack, fall and peak behaviour.
  const float* levels() const { return level_.data(); }
  float level(int band) const { return level_[band]; }

  float floorDb(int band) const { return floorDb_[band]; }

  // Learn the room's noise for kLearnNoiseSeconds, whatever is playing.
  void learnNoise() { learnSecondsLeft_ = config::kLearnNoiseSeconds; }
  bool learningNoise() const { return learnSecondsLeft_ > 0; }
  float gateDb(int band) const;   // floor plus the band's noise margin

 private:
  int bands_ = 0;
  Buffer<float> tiltDb_;
  Buffer<float> floorDb_;
  Buffer<float> noiseVarDb2_;   // variance of the band's level around its floor, dB^2
  Buffer<float> smoothDb_;      // band level averaged over ~1 s
  Buffer<float> bandDb_;        // this frame's level per band, dB
  Buffer<float> level_;         // output, 0..1 per band
  float broadFastDb_ = -120, broadSlowDb_ = -120, broadDeviationDb_ = 0;
  float quietRefDb_ = config::kQuietCeilingDb;
  float agcRefDb_ = -120;
  float silentSeconds_ = 0;
  bool silent_ = true;
  uint16_t lastActive_ = 0;
  float learnSecondsLeft_ = 0;
  float warmupSeconds_ = 0;   // counts up to kWarmupSeconds after begin()
};
