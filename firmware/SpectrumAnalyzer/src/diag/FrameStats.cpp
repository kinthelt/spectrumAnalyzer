#include "FrameStats.h"

#include <Arduino.h>

#include "../util/Log.h"

void FrameStats::recordFrame(uint32_t showMicros) {
  ++frames_;
  if (showMicros > maxShowMicros_) maxShowMicros_ = showMicros;
}

void FrameStats::logIfDue(uint32_t nowMillis) {
  const uint32_t elapsed = nowMillis - windowStartMillis_;
  if (elapsed < intervalMillis_) return;

  LOG_INFO("stats", "%.1f fps, show() max %lu us, heap free %lu B, PSRAM free %lu B",
           frames_ * 1000.0f / elapsed, (unsigned long)maxShowMicros_,
           (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());

  windowStartMillis_ = nowMillis;
  frames_ = 0;
  maxShowMicros_ = 0;
}
