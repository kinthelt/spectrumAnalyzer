// Counts frames and the slowest LED update, and logs a summary periodically.
#pragma once

#include <cstdint>

class FrameStats {
 public:
  explicit FrameStats(uint32_t intervalMillis) : intervalMillis_(intervalMillis) {}

  void begin(uint32_t nowMillis) { windowStartMillis_ = nowMillis; }

  // Call once per frame with how long the LED update took.
  void recordFrame(uint32_t showMicros);

  // Logs and resets the counters once per interval.
  void logIfDue(uint32_t nowMillis);

 private:
  uint32_t intervalMillis_;
  uint32_t windowStartMillis_ = 0;
  uint32_t frames_ = 0;
  uint32_t maxShowMicros_ = 0;
};
