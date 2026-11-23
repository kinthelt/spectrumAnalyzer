// Text commands over Serial Monitor for changing settings on the bench.
// Type "help" for the list. Stays useful for debugging after the web page exists.
#pragma once

#include <cstddef>

#include "../settings/SettingsManager.h"
#include "AudioMonitor.h"

class SerialConsole {
 public:
  SerialConsole(SettingsManager& settings, AudioMonitor& audioMonitor)
      : settings_(settings), audioMonitor_(audioMonitor) {}

  // Reads any waiting characters and runs complete lines. Never blocks.
  void poll();

  void printHelp();
  void printStatus();

 private:
  void runLine(char* line);
  bool parseSlot(const char* text, int* slot);   // "1".."8" -> 0..7

  static constexpr size_t kMaxLine = 64;
  SettingsManager& settings_;
  AudioMonitor& audioMonitor_;
  char line_[kMaxLine];
  size_t length_ = 0;
  bool resetArmed_ = false;
};
