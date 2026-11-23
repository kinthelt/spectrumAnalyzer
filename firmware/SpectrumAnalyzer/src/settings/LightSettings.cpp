#include "LightSettings.h"

#include <cstdio>
#include <cstring>

namespace {

const char* const kModeNames[kModeCount] = {
    "spectrum-snappy", "spectrum-smooth", "solid", "gradient", "rainbow", "twinkle",
};

const char* const kSpectrumColourNames[kSpectrumColourCount] = {"rainbow", "fixed"};

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

const char* modeName(Mode mode) {
  const uint8_t i = static_cast<uint8_t>(mode);
  return i < kModeCount ? kModeNames[i] : "unknown";
}

bool modeFromName(const char* name, Mode* out) {
  for (uint8_t i = 0; i < kModeCount; ++i) {
    if (std::strcmp(name, kModeNames[i]) == 0) {
      *out = static_cast<Mode>(i);
      return true;
    }
  }
  return false;
}

const char* spectrumColourName(SpectrumColour sc) {
  const uint8_t i = static_cast<uint8_t>(sc);
  return i < kSpectrumColourCount ? kSpectrumColourNames[i] : "unknown";
}

bool spectrumColourFromName(const char* name, SpectrumColour* out) {
  for (uint8_t i = 0; i < kSpectrumColourCount; ++i) {
    if (std::strcmp(name, kSpectrumColourNames[i]) == 0) {
      *out = static_cast<SpectrumColour>(i);
      return true;
    }
  }
  return false;
}

bool parseHexColour(const char* text, Rgb* out) {
  if (text[0] == '#') ++text;
  if (std::strlen(text) != 6) return false;
  uint8_t bytes[3];
  for (int i = 0; i < 3; ++i) {
    const int hi = hexDigit(text[2 * i]);
    const int lo = hexDigit(text[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  *out = Rgb{bytes[0], bytes[1], bytes[2]};
  return true;
}

void formatHexColour(const Rgb& c, char* buf) {
  std::snprintf(buf, 7, "%02X%02X%02X", c.r, c.g, c.b);
}
