// Keeps the strip setup (StripSetup) in flash under "leds", and tells the web
// page when a change needs a restart. "Reset settings" leaves it alone, like Wi-Fi.
// Plain C++ apart from the SettingsStore it is given, so it is unit-tested.
#pragma once

#include <string>

#include "SettingsStore.h"
#include "StripSetup.h"

class StripSetupManager {
 public:
  explicit StripSetupManager(SettingsStore& store) : store_(store) {}

  // Reads it from flash (missing or unreadable: the defaults). What it reads is
  // what the lights start with.
  void begin();

  const StripSetup& chosen() const { return chosen_; }     // as set on the page
  const StripSetup& running() const { return running_; }   // what the lights are using

  // A change from the web page. Returns "" and saves it, or what's wrong (then
  // nothing changes). needsRestart: the LED count differs from what's running.
  std::string change(const StripSetup& next, bool* needsRestart);

 private:
  SettingsStore& store_;
  StripSetup chosen_;
  StripSetup running_;
};
