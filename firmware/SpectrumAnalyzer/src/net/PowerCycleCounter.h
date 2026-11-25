// Spots "switched off at the plug several times in a row, quickly": a way to
// open the Wi-Fi setup network when the board is out of reach above the ceiling.
//
// Each power-up adds one to a counter in flash. Staying on for kPowerCycleWindowMillis
// clears it. Reaching kPowerCyclesForSetup triggers setup and clears it.
// Only real power-ups count: a crash restarted the board twice quickly on the
// bench and was taken for the plug trick. Restarts for updates, a new strip
// setup or a crash clear the count instead.
// Plain C++ apart from the SettingsStore it is given, so it is unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../settings/SettingsStore.h"

class PowerCycleCounter {
 public:
  explicit PowerCycleCounter(SettingsStore& store) : store_(store) {}

  // Call once at boot. poweredUp: the reset was a power-up (power-on or brownout),
  // not a software restart, crash or watchdog. Returns true if this boot
  // completes the sequence.
  bool begin(bool poweredUp);

  // Call every loop; clears the count once the board has stayed on long enough.
  void tick(uint32_t nowMillis);

  int count() const { return count_; }

 private:
  void write(uint8_t value);

  static constexpr const char* kKey = "pwrcyc";
  SettingsStore& store_;
  int count_ = 0;
  bool cleared_ = false;
};
