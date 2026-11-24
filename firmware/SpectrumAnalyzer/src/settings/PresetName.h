// Preset names: tidying what people type (often on a phone keyboard) into
// something safe to store and show. Plain C++ so it is unit-tested on a PC.
#pragma once

#include <cstddef>

constexpr size_t kPresetNameMaxBytes = 24;   // UTF-8 bytes, so fewer for accented letters/emoji

// Writes a tidied copy of `in` into out (at least kPresetNameMaxBytes + 1 bytes):
//   - control characters removed, leading/trailing spaces trimmed;
//   - cut to kPresetNameMaxBytes without splitting a multi-byte character;
//   - empty (or nullptr) becomes the default, "Preset <slot + 1>".
void tidyPresetName(const char* in, int slot, char* out);

// The name a preset slot has when it has never been named.
void defaultPresetName(int slot, char* out);
