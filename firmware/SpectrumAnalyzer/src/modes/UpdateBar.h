// What the strip shows during a firmware update, drawn over (instead of) the
// current mode:
//   Receiving  a blue bar filling left to right over a faint track
//   Succeeded  green flashes until the board restarts
//   Failed     red flashes for kUpdateFailFlashMillis, then the mode returns
// Plain C++, unit-tested on a PC.
#pragma once

#include <cstdint>

#include "../update/UpdateProgress.h"
#include "../util/Rgb.h"

namespace update_bar {

constexpr Rgb kBar{0, 110, 255};
constexpr Rgb kTrack{5, 5, 9};
constexpr Rgb kGood{0, 255, 40};
constexpr Rgb kBad{255, 0, 0};
constexpr uint32_t kFlashHalfPeriodMillis = 250;

// Draws the update display into out and returns true, or returns false (out
// untouched) when there's nothing to show and the mode should draw as usual.
bool render(const UpdateProgress& progress, uint32_t nowMillis, uint32_t failFlashMillis, Rgb* out,
            int count);

}  // namespace update_bar
