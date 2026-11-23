// Converts LightSettings to and from the bytes stored in flash.
//
// The layout is explicit (not a raw struct copy) and starts with a version byte,
// so a firmware update can change the struct without misreading old data.
// Anything that fails validation is rejected, and the caller falls back to defaults.
#pragma once

#include <cstddef>
#include <cstdint>

#include "LightSettings.h"

namespace settings_codec {

constexpr uint8_t kVersion = 1;
constexpr size_t kEncodedSize = 11;

// Writes exactly kEncodedSize bytes.
void encode(const LightSettings& s, uint8_t* out);

// Returns false (and leaves *out untouched) for a wrong size, an unknown version
// or out-of-range values.
bool decode(const uint8_t* data, size_t length, LightSettings* out);

}  // namespace settings_codec
