// How the strip is installed: its length, which end the bass goes to, and the
// frequency range it shows. Set on the web page's Settings > Strip setup; kept
// in flash, apart from the light settings (not in presets, kept by "Reset
// settings", like Wi-Fi). Plain C++, unit-tested on a PC.
//
// Length and frequency range are used when the board starts (they size the
// filter bank and per-LED memory), so changing them restarts the lights.
// Which end the bass goes to applies at once.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../config/Config.h"

struct StripSetup {
  uint16_t ledCount = config::kDefaultLedCount;
  // false: bass on the left (as the strip is wired, LED 0 at the left);
  // true: everything mirrored, as if the strip were mounted the other way round.
  bool bassOnRight = false;
  float lowHz = config::kDefaultLowHz;
  float highHz = config::kDefaultHighHz;

  bool operator==(const StripSetup& o) const {
    return ledCount == o.ledCount && bassOnRight == o.bassOnRight && lowHz == o.lowHz && highHz == o.highHz;
  }
  bool operator!=(const StripSetup& o) const { return !(*this == o); }

  // Changing from this to other needs a restart (length or frequency range differ).
  bool needsRestartFor(const StripSetup& other) const {
    return ledCount != other.ledCount || lowHz != other.lowHz || highHz != other.highHz;
  }
};

namespace strip_setup {

// "" if usable, otherwise what's wrong, worded for the web page.
std::string validate(const StripSetup& s);

// Flash format (versioned, so later firmware can still read it).
constexpr size_t kEncodedSize = 12;
void encode(const StripSetup& s, uint8_t* out);
// False if the bytes aren't a valid setup (then out is left alone).
bool decode(const uint8_t* in, size_t length, StripSetup* out);

}  // namespace strip_setup
