// Keeps the strip setup (StripSetup) in flash and decides what start-up uses.
//
//   "strip"    the setup chosen on the web page
//   "stripok"  the last setup the lights started with successfully
//
// If the chosen setup can't be used at start-up (not enough memory), the lights
// use the last one that worked (or the defaults) and the web page says so.
// "Reset settings" leaves both alone, like Wi-Fi.
// Plain C++ apart from the SettingsStore it is given, so it is unit-tested.
#pragma once

#include <string>

#include "SettingsStore.h"
#include "StripSetup.h"

class StripSetupManager {
 public:
  explicit StripSetupManager(SettingsStore& store) : store_(store) {}

  // Reads both from flash. Missing or unreadable ones are the defaults.
  void begin();

  const StripSetup& chosen() const { return chosen_; }
  const StripSetup& lastWorking() const { return lastWorking_; }

  // Start-up reports which setup the lights are running with. If it isn't the
  // chosen one, fellBack() is true until the next start-up or change.
  void setRunning(const StripSetup& running);
  const StripSetup& running() const { return running_; }
  bool fellBack() const { return fellBack_; }

  // A change from the web page. Returns "" and saves it, or what's wrong (then
  // nothing changes). needsRestart: the length or frequency range differ from
  // what's running, so the lights must restart to use them.
  std::string change(const StripSetup& next, bool* needsRestart);

 private:
  bool read(const char* key, StripSetup* out);
  void write(const char* key, const StripSetup& s);

  SettingsStore& store_;
  StripSetup chosen_;
  StripSetup lastWorking_;
  StripSetup running_;
  bool fellBack_ = false;
};
