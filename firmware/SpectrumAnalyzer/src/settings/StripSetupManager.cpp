#include "StripSetupManager.h"

namespace {
constexpr const char* kKey = "leds";
}  // namespace

void StripSetupManager::begin() {
  // Keys left by the step 10 trial builds (length and frequency range); unused now.
  store_.remove("strip");
  store_.remove("stripok");

  chosen_ = StripSetup{};
  uint8_t buf[strip_setup::kEncodedSize + 1];
  strip_setup::decode(buf, store_.read(kKey, buf, sizeof(buf)), &chosen_);
  running_ = chosen_;
}

std::string StripSetupManager::change(const StripSetup& next, bool* needsRestart) {
  const std::string err = strip_setup::validate(next);
  if (!err.empty()) return err;
  *needsRestart = next.ledCount != running_.ledCount;
  if (next != chosen_) {
    chosen_ = next;
    uint8_t buf[strip_setup::kEncodedSize];
    strip_setup::encode(next, buf);
    store_.write(kKey, buf, sizeof(buf));
  }
  running_.bassOnRight = next.bassOnRight;   // applies at once
  return "";
}
