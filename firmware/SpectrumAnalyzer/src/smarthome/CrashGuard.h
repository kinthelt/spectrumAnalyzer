// Stops a crash loop caused by Apple Home / Google Home (Matter) from making the
// lights unreachable in the ceiling.
//
// Matter is "on trial" from when it starts until it has run kMatterStableMillis.
// A crash (or watchdog) during that time counts; kMatterCrashesToPause in a row and
// the next start-ups skip Matter: lights, web page and updates keep working, and
// the Settings page says Home is paused and offers to turn it back on.
//
// The state lives in RAM that survives restarts and crashes but not a power cut,
// so switching off at the plug also gives Matter a fresh start.
// Plain C++ (the caller keeps State in that RAM), unit-tested on a PC.
#pragma once

#include <cstdint>

class CrashGuard {
 public:
  struct State {
    uint32_t magic;     // kMagic once set up (RAM holds junk after a power cut)
    uint8_t onTrial;    // Matter started and hasn't yet run long enough
    uint8_t crashes;    // crashes in a row while on trial
  };
  static constexpr uint32_t kMagic = 0x4D545231;   // "MTR1"

  explicit CrashGuard(State& state) : s_(state) {}

  // At start-up. crashed: the last reset was a crash or watchdog.
  void begin(bool crashed);
  // Matter may start this time.
  bool allowed() const;
  int crashes() const { return s_.crashes; }

  void starting();   // just before Matter starts
  void stable();     // Matter has run kMatterStableMillis
  void reset();      // "turn Home back on": allowed again from the next start-up

 private:
  State& s_;
};
