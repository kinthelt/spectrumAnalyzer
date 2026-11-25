// Live text view of the audio analysis in Serial Monitor (toggle with "audio").
//
//   -52.3 dBFS  ref  -61.0  quiet -77.4  lit  37       |  .:-=+*#%@%#*=-:.    ...  |
//
// Up to 50 characters, each the loudest of its share of the LEDs (bass on the
// left, as the strip is wired), from ' ' (off) to '@' (full). Printed 5 times a second.
#pragma once

#include <cstdint>

#include "../audio/AudioFrame.h"
#include "../util/Memory.h"

class AudioMonitor {
 public:
  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }

  // Sizes the level buffer for count LEDs. False if out of memory.
  bool allocate(int count) { return levels_.allocate(count); }
  // Where the caller copies a frame's levels before onFrame().
  float* levels() { return levels_.data(); }

  // Call with every new frame (its levels already in levels()).
  void onFrame(const AudioFrame& frame, uint32_t nowMillis);

 private:
  Buffer<float> levels_;
  bool enabled_ = false;
  uint32_t lastPrintMillis_ = 0;
};
