// The LED task: its own FreeRTOS task on core 1, above the Arduino loop's
// priority, drawing one frame every 10 ms on a fixed beat.
//
// It owns everything LED-side: taking audio frames, the modes, FastLED, and the
// frame statistics. It reads settings only through a shared snapshot that the
// Arduino loop publishes, so network work in the loop (web page, Wi-Fi setup)
// can never delay a frame, and the two never touch the same data unguarded.
// During a firmware update it shows the update colours instead of the mode
// (see modes/UpdateBar.h).
#pragma once

#include "../audio/AudioInput.h"
#include "../config/Config.h"
#include "../diag/FrameStats.h"
#include "../modes/ModeManager.h"
#include "../settings/LightSettings.h"
#include "../update/UpdateProgress.h"
#include "../util/Shared.h"
#include "LedOutput.h"

class LedTask {
 public:
  LedTask(AudioInput& audio, const Shared<LightSettings>& settings,
          const Shared<UpdateProgress>& update)
      : audio_(audio), settings_(settings), update_(update) {}

  // Sets up FastLED and starts the task. Call once from setup(), after the
  // first settings snapshot has been published.
  void start();

 private:
  static void entry(void* self);
  void run();
  void drawFrame(uint32_t nowMillis);

  AudioInput& audio_;
  const Shared<LightSettings>& settings_;
  const Shared<UpdateProgress>& update_;

  LedOutput leds_;
  ModeManager modes_;
  FrameStats stats_{config::kStatsIntervalMillis};

  LightSettings current_;
  unsigned settingsVersion_ = 0;
  UpdateProgress updateProgress_;
  unsigned updateVersion_ = 0;
  bool showingUpdate_ = false;
  int heldFrames_ = 0;   // frames sent since the strip started holding the update colour
  AudioFrame audioFrame_;
  bool haveAudio_ = false;
  uint32_t lastAudioMillis_ = 0;
  Rgb frame_[config::kNumLeds];
};
