#include "FirmwareMarker.h"

using firmware_marker::kLength;
using firmware_marker::kText;

MarkerScanner::MarkerScanner() {
  // failure_[i]: length of the longest proper prefix of kText[0..i] that is also its suffix.
  failure_[0] = 0;
  size_t k = 0;
  for (size_t i = 1; i < kLength; ++i) {
    while (k > 0 && kText[i] != kText[k]) k = failure_[k - 1];
    if (kText[i] == kText[k]) ++k;
    failure_[i] = k;
  }
}

void MarkerScanner::feed(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len && !found_; ++i) {
    const char c = static_cast<char>(data[i]);
    while (matched_ > 0 && c != kText[matched_]) matched_ = failure_[matched_ - 1];
    if (c == kText[matched_]) ++matched_;
    if (matched_ == kLength) found_ = true;
  }
}
