#include "StripSetupManager.h"

namespace {
constexpr const char* kChosenKey = "strip";
constexpr const char* kWorkingKey = "stripok";
}  // namespace

bool StripSetupManager::read(const char* key, StripSetup* out) {
  uint8_t buf[strip_setup::kEncodedSize + 1];
  const size_t n = store_.read(key, buf, sizeof(buf));
  return strip_setup::decode(buf, n, out);
}

void StripSetupManager::write(const char* key, const StripSetup& s) {
  uint8_t buf[strip_setup::kEncodedSize];
  strip_setup::encode(s, buf);
  store_.write(key, buf, sizeof(buf));
}

void StripSetupManager::begin() {
  chosen_ = StripSetup{};
  read(kChosenKey, &chosen_);
  lastWorking_ = StripSetup{};
  read(kWorkingKey, &lastWorking_);
  running_ = chosen_;
  fellBack_ = false;
}

void StripSetupManager::setRunning(const StripSetup& running) {
  running_ = running;
  fellBack_ = running.needsRestartFor(chosen_);
  if (!fellBack_ && running != lastWorking_) {
    lastWorking_ = running;
    write(kWorkingKey, running);
  }
}

std::string StripSetupManager::change(const StripSetup& next, bool* needsRestart) {
  const std::string err = strip_setup::validate(next);
  if (!err.empty()) return err;
  *needsRestart = running_.needsRestartFor(next);
  if (next != chosen_) {
    chosen_ = next;
    write(kChosenKey, next);
  }
  running_.bassOnRight = next.bassOnRight;   // applies at once
  if (!*needsRestart) fellBack_ = false;
  return "";
}
