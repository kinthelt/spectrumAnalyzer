// A fixed text that every build of this firmware contains, used to refuse
// uploads of some other sketch's firmware (the Wi-Fi/Matter spike, an example).
//
// Nothing needs to put it in the firmware specially: MarkerScanner searches for
// this very array, so the array (and so the text) is always in the build.
// Leave the text alone: firmware already on the board looks for its own text,
// so after a change it would refuse the new build over Wi-Fi (USB still works).
#pragma once

#include <cstddef>
#include <cstdint>

namespace firmware_marker {
constexpr char kText[] = "spectrumAnalyzer-firmware-marker:4c9e27b1";
constexpr size_t kLength = sizeof(kText) - 1;
}  // namespace firmware_marker

// Looks for the marker in a stream of chunks, including across chunk boundaries.
class MarkerScanner {
 public:
  MarkerScanner();
  void reset() {
    matched_ = 0;
    found_ = false;
  }
  void feed(const uint8_t* data, size_t len);
  bool found() const { return found_; }

 private:
  size_t failure_[firmware_marker::kLength];   // Knuth-Morris-Pratt table
  size_t matched_ = 0;
  bool found_ = false;
};
