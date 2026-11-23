// Counts LED frames and audio blocks, tracks the slowest of each, and logs a
// summary periodically.
#pragma once

#include <cstdint>

class FrameStats {
 public:
  explicit FrameStats(uint32_t intervalMillis) : intervalMillis_(intervalMillis) {}

  void begin(uint32_t nowMillis) { windowStartMillis_ = nowMillis; }

  // Call once per frame with how long the LED update took.
  void recordFrame(uint32_t showMicros);

  // Call once per audio frame received, with the time its analysis took.
  void recordAudio(uint32_t dspMicros);

  // Logs and resets the counters once per interval.
  void logIfDue(uint32_t nowMillis);

 private:
  uint32_t intervalMillis_;
  uint32_t windowStartMillis_ = 0;
  uint32_t frames_ = 0;
  uint32_t maxShowMicros_ = 0;
  uint32_t audioFrames_ = 0;
  uint32_t maxDspMicros_ = 0;
  uint64_t totalDspMicros_ = 0;
};
