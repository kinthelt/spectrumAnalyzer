#include "FrameStats.h"

#include <Arduino.h>

#include "../config/Config.h"
#include "../util/Log.h"

void FrameStats::recordFrame(uint32_t showMicros) {
  ++frames_;
  if (showMicros > maxShowMicros_) maxShowMicros_ = showMicros;
}

void FrameStats::recordAudio(uint32_t dspMicros) {
  ++audioFrames_;
  totalDspMicros_ += dspMicros;
  if (dspMicros > maxDspMicros_) maxDspMicros_ = dspMicros;
}

void FrameStats::logIfDue(uint32_t nowMillis) {
  const uint32_t elapsed = nowMillis - windowStartMillis_;
  if (elapsed < intervalMillis_) return;

  // "lowest": the least internal RAM there has been since start-up. If it keeps
  // falling over days, something is leaking.
  LOG_INFO("stats", "%.1f fps, show() max %lu us, heap free %lu B (lowest %lu B), PSRAM free %lu B",
           frames_ * 1000.0f / elapsed, (unsigned long)maxShowMicros_, (unsigned long)ESP.getFreeHeap(),
           (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getFreePsram());

  if (audioFrames_ > 0) {
    // Load = share of each block's real-time budget spent analysing it.
    constexpr float kBlockMicros = 1e6f * config::kAudioBlockSize / config::kSampleRate;
    const float avgDsp = static_cast<float>(totalDspMicros_) / audioFrames_;
    LOG_INFO("stats", "audio: %.1f blocks/s, analysis avg %.0f us (%.0f%% of core 0), max %lu us",
             audioFrames_ * 1000.0f / elapsed, avgDsp, 100.0f * avgDsp / kBlockMicros,
             (unsigned long)maxDspMicros_);
  } else {
    LOG_WARN("stats", "audio: no frames received");
  }

  windowStartMillis_ = nowMillis;
  frames_ = 0;
  maxShowMicros_ = 0;
  audioFrames_ = 0;
  maxDspMicros_ = 0;
  totalDspMicros_ = 0;
}
