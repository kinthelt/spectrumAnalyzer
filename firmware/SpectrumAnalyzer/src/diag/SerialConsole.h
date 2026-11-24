// Text commands over Serial Monitor for changing settings on the bench.
// Type "help" for the list. Stays useful for debugging after the web page exists.
#pragma once

#include <cstddef>

#include "../audio/AudioInput.h"
#include "../net/Connectivity.h"
#include "../settings/SettingsManager.h"
#include "AudioMonitor.h"

class SerialConsole {
 public:
  SerialConsole(SettingsManager& settings, AudioInput& audio, AudioMonitor& audioMonitor,
                Connectivity& connectivity)
      : settings_(settings), audio_(audio), audioMonitor_(audioMonitor), connectivity_(connectivity) {}

  // Reads any waiting characters and runs complete lines. Never blocks.
  void poll();

  void printHelp();
  void printStatus();

 private:
  void runLine(char* line);
  bool parseSlot(const char* text, int* slot);   // "1".."8" -> 0..7

  static constexpr size_t kMaxLine = 64;
  SettingsManager& settings_;
  AudioInput& audio_;
  AudioMonitor& audioMonitor_;
  Connectivity& connectivity_;
  char line_[kMaxLine];
  size_t length_ = 0;
  bool resetArmed_ = false;
};
