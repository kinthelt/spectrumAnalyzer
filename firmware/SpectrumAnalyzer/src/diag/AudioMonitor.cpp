#include "AudioMonitor.h"

#include <Arduino.h>

namespace {
constexpr uint32_t kPrintIntervalMillis = 200;
constexpr int kMaxColumns = 50;
const char kRamp[] = " .:-=+*#%@";   // 10 steps, off to full
}  // namespace

void AudioMonitor::onFrame(const AudioFrame& frame, uint32_t nowMillis) {
  if (!enabled_ || nowMillis - lastPrintMillis_ < kPrintIntervalMillis) return;
  lastPrintMillis_ = nowMillis;

  const int count = static_cast<int>(levels_.size());
  const int perColumn = (count + kMaxColumns - 1) / kMaxColumns;
  const int columns = perColumn > 0 ? (count + perColumn - 1) / perColumn : 0;
  char bar[kMaxColumns + 1];
  for (int c = 0; c < columns; ++c) {
    float level = 0;
    for (int k = 0; k < perColumn; ++k) {
      const int i = c * perColumn + k;
      if (i < count && levels_[i] > level) level = levels_[i];
    }
    int step = static_cast<int>(level * 9.0f + 0.5f);
    if (level > 0 && step == 0) step = 1;   // anything lit shows at least '.'
    bar[c] = kRamp[step];
  }
  bar[columns] = '\0';

  const char* state = frame.learningNoise ? "LEARN" : (frame.silent ? "SILENT" : "");
  Serial.printf("%6.1f dBFS  ref %6.1f  quiet %6.1f  lit %3u %-6s |%s|\n", frame.broadbandDb,
                frame.agcRefDb, frame.quietRefDb, frame.activeBands, state, bar);
}
