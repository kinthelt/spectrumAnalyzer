// One analysis result's summary, published by the audio task about 94 times a
// second. The per-LED levels travel separately (AudioInput::latest), because
// their number is the strip length, a setting.
#pragma once

#include <cstdint>

struct AudioFrame {
  float broadbandDb = -120;   // overall level, dBFS (sum of all bands)
  float agcRefDb = -120;      // top of the display window (loudest band, tilted dB)
  float quietRefDb = -120;    // estimate of the room's quiet level, dBFS
  uint16_t activeBands = 0;   // bands with level > 0
  bool silent = true;         // see kSilenceSeconds in Config.h
  bool learningNoise = false; // a "learn room noise" request is running

  uint32_t sequence = 0;      // increments with every frame
  uint32_t dspMicros = 0;     // time spent analysing this block (filled by AudioInput)
};
