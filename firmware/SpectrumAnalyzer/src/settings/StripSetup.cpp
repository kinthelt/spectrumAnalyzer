#include "StripSetup.h"

namespace strip_setup {

namespace {
constexpr uint8_t kFormatVersion = 1;
}

std::string validate(const StripSetup& s) {
  if (s.ledCount < config::kMinLedCount || s.ledCount > config::kMaxLedCount) {
    return "The number of LEDs must be " + std::to_string(config::kMinLedCount) + " to " +
           std::to_string(config::kMaxLedCount) + ".";
  }
  return "";
}

// Layout: version, LED count (2 bytes, little-endian), flags (bit 0: bass on the right).
void encode(const StripSetup& s, uint8_t* out) {
  out[0] = kFormatVersion;
  out[1] = static_cast<uint8_t>(s.ledCount & 0xFF);
  out[2] = static_cast<uint8_t>(s.ledCount >> 8);
  out[3] = s.bassOnRight ? 1 : 0;
}

bool decode(const uint8_t* in, size_t length, StripSetup* out) {
  if (length != kEncodedSize || in[0] != kFormatVersion || in[3] > 1) return false;
  StripSetup s;
  s.ledCount = static_cast<uint16_t>(in[1] | (in[2] << 8));
  s.bassOnRight = in[3] == 1;
  if (!validate(s).empty()) return false;
  *out = s;
  return true;
}

}  // namespace strip_setup
