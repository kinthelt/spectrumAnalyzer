// Live text view of the audio analysis in Serial Monitor (toggle with "audio").
//
//   -52.3 dBFS  ref  -61.0  quiet -77.4  lit  37       |  .:-=+*#%@%#*=-:.    ...  |
//
// 50 characters, each the loudest of 3 neighbouring LEDs (bass on the left),
// from ' ' (off) to '@' (full). Printed 5 times a second.
#pragma once

#include <cstdint>

#include "../audio/AudioFrame.h"

class AudioMonitor {
 public:
  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }

  // Call with every new frame.
  void onFrame(const AudioFrame& frame, uint32_t nowMillis);

 private:
  bool enabled_ = false;
  uint32_t lastPrintMillis_ = 0;
};
