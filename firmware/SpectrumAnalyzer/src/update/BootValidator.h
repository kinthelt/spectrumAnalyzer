// Decides whether firmware that has just been installed over Wi-Fi is good.
//
// The bootloader starts a new firmware "on trial". If the trial firmware
// restarts (crash, power cut) before declaring itself good, the bootloader goes
// back to the previous firmware by itself. This class decides when to declare
// it good, or to give up early and go back:
//   - good: on the home Wi-Fi with the web page running for kUpdateGoodAfterMillis
//     without a break;
//   - go back: that hasn't happened kUpdateGiveUpMillis after starting.
// Plain C++, unit-tested on a PC; FirmwareUpdate carries out the decision.
#pragma once

#include <cstdint>

class BootValidator {
 public:
  enum class Action : uint8_t { None, MarkGood, RollBack };

  BootValidator(uint32_t goodAfterMillis, uint32_t giveUpMillis)
      : goodAfter_(goodAfterMillis), giveUp_(giveUpMillis) {}

  // onTrial: this firmware was just installed and hasn't been declared good yet.
  void begin(bool onTrial, uint32_t nowMillis);

  // healthy: on the home Wi-Fi with the web page running. Call every loop.
  // Returns MarkGood or RollBack once, then None.
  Action update(bool healthy, uint32_t nowMillis);

  bool onTrial() const { return onTrial_; }

 private:
  const uint32_t goodAfter_;
  const uint32_t giveUp_;
  bool onTrial_ = false;
  uint32_t startMillis_ = 0;
  bool healthy_ = false;
  uint32_t healthySince_ = 0;
};
