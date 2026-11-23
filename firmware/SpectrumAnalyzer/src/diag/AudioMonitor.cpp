#include "AudioMonitor.h"

#include <Arduino.h>

namespace {
constexpr uint32_t kPrintIntervalMillis = 200;
constexpr int kColumns = 50;
constexpr int kBandsPerColumn = (config::kNumLeds + kColumns - 1) / kColumns;
const char kRamp[] = " .:-=+*#%@";   // 10 steps, off to full
}  // namespace

void AudioMonitor::onFrame(const AudioFrame& frame, uint32_t nowMillis) {
  if (!enabled_ || nowMillis - lastPrintMillis_ < kPrintIntervalMillis) return;
  lastPrintMillis_ = nowMillis;

  char bar[kColumns + 1];
  for (int c = 0; c < kColumns; ++c) {
    float level = 0;
    for (int k = 0; k < kBandsPerColumn; ++k) {
      const int i = c * kBandsPerColumn + k;
      if (i < config::kNumLeds && frame.level[i] > level) level = frame.level[i];
    }
    int step = static_cast<int>(level * 9.0f + 0.5f);
    if (level > 0 && step == 0) step = 1;   // anything lit shows at least '.'
    bar[c] = kRamp[step];
  }
  bar[kColumns] = '\0';

  Serial.printf("%6.1f dBFS  ref %6.1f  quiet %6.1f  lit %3u %-6s |%s|\n", frame.broadbandDb,
                frame.agcRefDb, frame.quietRefDb, frame.activeBands,
                frame.silent ? "SILENT" : "", bar);
}
