#include "SettingsCodec.h"

namespace settings_codec {

// Layout, version 1:
//   0  version
//   1  mode
//   2  spectrum colour
//   3  on (0/1)
//   4  brightness
//   5  colour A r, g, b
//   8  colour B r, g, b

void encode(const LightSettings& s, uint8_t* out) {
  out[0] = kVersion;
  out[1] = static_cast<uint8_t>(s.mode);
  out[2] = static_cast<uint8_t>(s.spectrumColour);
  out[3] = s.on ? 1 : 0;
  out[4] = s.brightness;
  out[5] = s.colourA.r;
  out[6] = s.colourA.g;
  out[7] = s.colourA.b;
  out[8] = s.colourB.r;
  out[9] = s.colourB.g;
  out[10] = s.colourB.b;
}

bool decode(const uint8_t* data, size_t length, LightSettings* out) {
  if (length != kEncodedSize || data[0] != kVersion) return false;
  if (data[1] >= kModeCount || data[2] >= kSpectrumColourCount || data[3] > 1) return false;

  LightSettings s;
  s.mode = static_cast<Mode>(data[1]);
  s.spectrumColour = static_cast<SpectrumColour>(data[2]);
  s.on = data[3] == 1;
  s.brightness = data[4];
  s.colourA = Rgb{data[5], data[6], data[7]};
  s.colourB = Rgb{data[8], data[9], data[10]};
  *out = s;
  return true;
}

}  // namespace settings_codec
