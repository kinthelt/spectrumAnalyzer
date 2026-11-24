// What the strip shows during a firmware update, instead of the current mode:
//   Receiving  one steady dim blue, sent a few times and then held (see below)
//   Succeeded  green flashes until the board restarts
//   Failed     red flashes for kUpdateFailFlashMillis, then the mode returns
//
// Why no live progress: writing the firmware to flash pauses both processor
// cores thousands of times, and data sent to the strip during a pause gets
// garbled (it flickered on the bench). So while the file arrives the LED task
// sends this colour for kHoldFrames frames, then stops sending; WS2812 LEDs keep
// showing the last colour they received. The flashes come after the writing.
// Plain C++, unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../update/UpdateProgress.h"
#include "../util/Rgb.h"

namespace update_bar {

constexpr Rgb kReceiving{0, 40, 110};
constexpr Rgb kGood{0, 255, 40};
constexpr Rgb kBad{255, 0, 0};
constexpr uint32_t kFlashHalfPeriodMillis = 250;
constexpr int kHoldFrames = 3;   // frames of kReceiving sent before the strip is left alone

// Draws the update display into out and returns true, or returns false (out
// untouched) when there's nothing to show and the mode should draw as usual.
bool render(const UpdateProgress& progress, uint32_t nowMillis, uint32_t failFlashMillis, Rgb* out,
            int count);

// True while the strip should be left holding its last colour after kHoldFrames.
inline bool holdsStill(const UpdateProgress& progress) {
  return progress.phase == UpdateProgress::Phase::Receiving;
}

}  // namespace update_bar
