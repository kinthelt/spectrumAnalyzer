#include "StripSetup.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace strip_setup {

namespace {
constexpr uint8_t kFormatVersion = 1;

std::string hz(float f) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%g", f);
  return buf;
}
}  // namespace

std::string validate(const StripSetup& s) {
  if (s.ledCount < config::kMinLedCount || s.ledCount > config::kMaxLedCount) {
    return "The number of LEDs must be " + std::to_string(config::kMinLedCount) + " to " +
           std::to_string(config::kMaxLedCount) + ".";
  }
  if (!(s.lowHz >= config::kLowestHz) || !(s.highHz <= config::kHighestHz)) {
    return "The frequencies must be between " + hz(config::kLowestHz) + " Hz and " + hz(config::kHighestHz) +
           " Hz.";
  }
  if (!(s.highHz >= 2.0f * s.lowHz)) {
    return "The highest frequency must be at least double the lowest (one octave).";
  }
  return "";
}

// Layout: version, count (2 bytes), flags, low Hz (float), high Hz (float). Little-endian.
void encode(const StripSetup& s, uint8_t* out) {
  out[0] = kFormatVersion;
  out[1] = static_cast<uint8_t>(s.ledCount & 0xFF);
  out[2] = static_cast<uint8_t>(s.ledCount >> 8);
  out[3] = s.bassOnRight ? 1 : 0;
  std::memcpy(&out[4], &s.lowHz, 4);
  std::memcpy(&out[8], &s.highHz, 4);
}

bool decode(const uint8_t* in, size_t length, StripSetup* out) {
  if (length != kEncodedSize || in[0] != kFormatVersion || in[3] > 1) return false;
  StripSetup s;
  s.ledCount = static_cast<uint16_t>(in[1] | (in[2] << 8));
  s.bassOnRight = in[3] == 1;
  std::memcpy(&s.lowHz, &in[4], 4);
  std::memcpy(&s.highHz, &in[8], 4);
  if (!validate(s).empty()) return false;
  *out = s;
  return true;
}

}  // namespace strip_setup
