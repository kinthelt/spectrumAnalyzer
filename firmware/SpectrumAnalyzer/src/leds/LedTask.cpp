#include "LedTask.h"

#include <Arduino.h>

#include "../modes/UpdateBar.h"
#include "../util/Log.h"

bool LedTask::allocate(int count) {
  count_ = 0;
  if (!leds_.allocate(count) || !frame_.allocate(count) || !levels_.allocate(count) || !modes_.allocate(count)) {
    return false;
  }
  count_ = count;
  return true;
}

void LedTask::start() {
  leds_.begin();
  settingsVersion_ = settings_.get(&current_);
  leds_.setBrightness(current_.brightness);
  stats_.begin(millis());

  if (xTaskCreatePinnedToCore(entry, "leds", config::kLedTaskStackBytes, this,
                              config::kLedTaskPriority, nullptr, config::kLedTaskCore) != pdPASS) {
    LOG_ERROR("leds", "could not start the LED task");
  }
}

void LedTask::entry(void* self) { static_cast<LedTask*>(self)->run(); }

void LedTask::run() {
  // A fixed beat: each frame starts exactly one period after the previous one
  // was due, so small delays elsewhere don't add up (they did when frames were
  // timed from the loop: 93.7 fps instead of 100).
  const TickType_t period = pdMS_TO_TICKS(config::kFrameIntervalMicros / 1000);
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, period);
    drawFrame(millis());
  }
}

void LedTask::drawFrame(uint32_t nowMillis) {
  // Newest audio frame from core 0, if one has arrived.
  if (audio_.latest(&audioFrame_, levels_.data())) {
    haveAudio_ = true;
    lastAudioMillis_ = nowMillis;
    stats_.recordAudio(audioFrame_.dspMicros);
  }

  // Settings changed in the loop (console, web page): take a copy.
  if (settings_.version() != settingsVersion_) {
    settingsVersion_ = settings_.get(&current_);
    if (!showingUpdate_) leds_.setBrightness(current_.brightness);
  }

  // Audio that has stopped arriving counts as none (the spectrum modes go dark).
  const bool audioFresh = haveAudio_ && nowMillis - lastAudioMillis_ < config::kAudioStaleMillis;
  modes_.render(current_, audioFresh ? &audioFrame_ : nullptr, levels_.data(), nowMillis, frame_.data(), count_);

  // A firmware update in progress replaces the mode's picture, at a brightness
  // that shows even if the lights were off or dimmed.
  if (update_.version() != updateVersion_) updateVersion_ = update_.get(&updateProgress_);
  const bool showUpdate = update_bar::render(updateProgress_, nowMillis, config::kUpdateFailFlashMillis,
                                             frame_.data(), count_);
  if (showUpdate != showingUpdate_) {
    showingUpdate_ = showUpdate;
    leds_.setBrightness(showUpdate ? config::kUpdateBrightness : current_.brightness);
  }
  // While the file is written to flash, send the steady colour a few times and
  // then leave the strip alone: frames sent during flash writes get garbled.
  if (showUpdate && update_bar::holdsStill(updateProgress_)) {
    if (heldFrames_ >= update_bar::kHoldFrames) return;
    ++heldFrames_;
  } else {
    heldFrames_ = 0;
  }
  leds_.write(frame_.data(), count_);
  leds_.show();

  stats_.recordFrame(leds_.lastShowMicros());
  stats_.logIfDue(nowMillis);
}
