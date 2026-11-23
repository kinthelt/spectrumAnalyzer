// Text commands over Serial Monitor for changing settings on the bench.
// Type "help" for the list. Stays useful for debugging after the web page exists.
#pragma once

#include <cstddef>

#include "../settings/SettingsManager.h"

class SerialConsole {
 public:
  explicit SerialConsole(SettingsManager& settings) : settings_(settings) {}

  // Reads any waiting characters and runs complete lines. Never blocks.
  void poll();

  void printHelp();
  void printStatus();

 private:
  void runLine(char* line);
  bool parseSlot(const char* text, int* slot);   // "1".."8" -> 0..7

  static constexpr size_t kMaxLine = 64;
  SettingsManager& settings_;
  char line_[kMaxLine];
  size_t length_ = 0;
  bool resetArmed_ = false;
};
