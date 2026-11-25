// How the strip is installed: how many LEDs are in use, and which end the bass
// goes to. Set on the web page's Settings > Strip setup; kept in flash apart from
// the light settings (not in presets, kept by "Reset settings", like Wi-Fi).
// The LED count is used when the board starts, so changing it restarts the
// lights; which end is bass applies at once. Plain C++, unit-tested on a PC.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../config/Config.h"

struct StripSetup {
  uint16_t ledCount = config::kMaxLedCount;
  // false: bass on the left (LED 0, where the data wire enters, on the left);
  // true: everything mirrored, as if the strip were mounted the other way round.
  bool bassOnRight = false;

  bool operator==(const StripSetup& o) const { return ledCount == o.ledCount && bassOnRight == o.bassOnRight; }
  bool operator!=(const StripSetup& o) const { return !(*this == o); }
};

namespace strip_setup {

// "" if usable, otherwise what's wrong, worded for the web page.
std::string validate(const StripSetup& s);

// Flash format (versioned, so later firmware can still read it).
constexpr size_t kEncodedSize = 4;
void encode(const StripSetup& s, uint8_t* out);
// False if the bytes aren't a valid setup (then out is left alone).
bool decode(const uint8_t* in, size_t length, StripSetup* out);

}  // namespace strip_setup
