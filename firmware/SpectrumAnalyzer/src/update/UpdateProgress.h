// What the LED task needs to know about a firmware update (shared from the loop).
#pragma once

#include <cstdint>

struct UpdateProgress {
  enum class Phase : uint8_t { Idle, Receiving, Succeeded, Failed };
  Phase phase = Phase::Idle;
  uint8_t percent = 0;            // while Receiving
  uint32_t phaseSinceMillis = 0;  // when this phase started (millis())
};
