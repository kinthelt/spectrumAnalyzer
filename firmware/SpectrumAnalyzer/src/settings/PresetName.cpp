#include "PresetName.h"

#include <cstdio>
#include <cstring>

void defaultPresetName(int slot, char* out) {
  std::snprintf(out, kPresetNameMaxBytes + 1, "Preset %d", slot + 1);
}

void tidyPresetName(const char* in, int slot, char* out) {
  size_t n = 0;
  if (in != nullptr) {
    // Copy, dropping control characters (and DEL).
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(in); *p != 0; ++p) {
      if (*p < 0x20 || *p == 0x7F) continue;
      if (n == kPresetNameMaxBytes) {
        // Full: if this byte continues a multi-byte character, the character
        // was cut. Back up to the start of that character and drop it.
        if ((*p & 0xC0) == 0x80) {
          while (n > 0 && (static_cast<unsigned char>(out[n - 1]) & 0xC0) == 0x80) --n;
          if (n > 0 && static_cast<unsigned char>(out[n - 1]) >= 0xC0) --n;
        }
        break;
      }
      out[n++] = static_cast<char>(*p);
    }
  }
  out[n] = '\0';

  // Trim spaces at both ends.
  size_t start = 0;
  while (out[start] == ' ') ++start;
  size_t end = std::strlen(out);
  while (end > start && out[end - 1] == ' ') --end;
  std::memmove(out, out + start, end - start);
  out[end - start] = '\0';

  if (out[0] == '\0') defaultPresetName(slot, out);
}
