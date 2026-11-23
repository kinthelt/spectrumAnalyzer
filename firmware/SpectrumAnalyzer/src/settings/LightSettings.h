// What the strip is set to: mode, colours, brightness, on/off.
// Plain C++ (no Arduino headers) so it can be unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../config/Config.h"
#include "../util/Rgb.h"

// Numbers are stored in flash: never renumber, only append.
enum class Mode : uint8_t {
  SpectrumSnappy = 0,
  SpectrumSmooth = 1,
  Solid = 2,
  Gradient = 3,
  Rainbow = 4,
  Twinkle = 5,
};
constexpr uint8_t kModeCount = 6;

// How the spectrum modes are coloured. Stored in flash: only append.
enum class SpectrumColour : uint8_t {
  RainbowCycle = 0,   // whole strip one hue, slowly cycling
  Fixed = 1,          // colour A
};
constexpr uint8_t kSpectrumColourCount = 2;

struct LightSettings {
  Mode mode = Mode::SpectrumSmooth;
  SpectrumColour spectrumColour = SpectrumColour::RainbowCycle;
  Rgb colourA{255, 160, 60};   // warm white-orange
  Rgb colourB{40, 80, 255};    // blue
  uint8_t brightness = config::kDefaultBrightness;   // 0-255
  bool on = true;

  bool operator==(const LightSettings& o) const {
    return mode == o.mode && spectrumColour == o.spectrumColour && colourA == o.colourA &&
           colourB == o.colourB && brightness == o.brightness && on == o.on;
  }
  bool operator!=(const LightSettings& o) const { return !(*this == o); }
};

const char* modeName(Mode mode);                     // "spectrum-snappy", "solid", ...
bool modeFromName(const char* name, Mode* out);      // accepts the names above
const char* spectrumColourName(SpectrumColour sc);   // "rainbow", "fixed"
bool spectrumColourFromName(const char* name, SpectrumColour* out);

// "RRGGBB" (optionally with a leading '#') -> colour. Returns false if malformed.
bool parseHexColour(const char* text, Rgb* out);
// Colour -> "RRGGBB" into buf (at least 7 bytes).
void formatHexColour(const Rgb& c, char* buf);
