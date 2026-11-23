// One analysis result, published by the audio task about 94 times a second.
#pragma once

#include <cstdint>

#include "../config/Config.h"

struct AudioFrame {
  // Per-LED level, 0 (at or below the noise floor / bottom of the range) to 1
  // (as loud as the loudest band). No smoothing beyond the filters: the spectrum
  // modes add their own attack, fall and peak behaviour.
  float level[config::kNumLeds] = {};

  float broadbandDb = -120;   // overall level, dBFS (sum of all bands)
  float agcRefDb = -120;      // top of the display window (loudest band, tilted dB)
  float quietRefDb = -120;    // estimate of the room's quiet level, dBFS
  uint16_t activeBands = 0;   // bands with level > 0
  bool silent = true;         // see kSilenceSeconds in Config.h
  bool learningNoise = false; // a "learn room noise" request is running

  uint32_t sequence = 0;      // increments with every frame
  uint32_t dspMicros = 0;     // time spent analysing this block (filled by AudioInput)
};
