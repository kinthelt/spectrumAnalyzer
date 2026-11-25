#include "PowerCycleCounter.h"

#include "../config/Config.h"

void PowerCycleCounter::write(uint8_t value) { store_.write(kKey, &value, 1); }

bool PowerCycleCounter::begin(bool poweredUp) {
  uint8_t stored = 0;
  if (store_.read(kKey, &stored, 1) != 1) stored = 0;
  if (!poweredUp) {
    if (stored != 0) write(0);
    count_ = 0;
    cleared_ = true;
    return false;
  }
  count_ = stored + 1;
  if (count_ >= config::kPowerCyclesForSetup) {
    write(0);
    cleared_ = true;
    return true;
  }
  write(static_cast<uint8_t>(count_));
  cleared_ = false;
  return false;
}

void PowerCycleCounter::tick(uint32_t nowMillis) {
  if (cleared_ || nowMillis < config::kPowerCycleWindowMillis) return;
  write(0);
  cleared_ = true;
}
