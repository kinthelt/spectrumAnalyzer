// Microphone input and analysis, running in its own task on core 0.
//
// The task reads 512-sample blocks from the ICS-43434 over I2S, runs the filter
// bank and level tracker, and publishes each result (a summary plus one level
// per LED) in a LevelsMailbox: the LED task takes new ones with latest(),
// anything else (live view, web page) reads with snapshot().
//
// If the analysis takes more than kAudioLoadLimit of a block's time (too many
// LEDs for the chip), the task gives core 0 a 1 ms break after the block, so
// core 0 never starves into a watchdog restart; the spectrum just gets choppier.
#pragma once

#include <atomic>

#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>

#include "../config/Config.h"
#include "../settings/StripSetup.h"
#include "AudioFrame.h"
#include "FilterBank.h"
#include "LevelTracker.h"
#include "LevelsMailbox.h"

class AudioInput {
 public:
  // Sizes and designs the analysis for the strip (one band per LED, over its
  // frequency range). False if out of memory or the range can't be built.
  bool allocate(const StripSetup& setup);

  // Sets up I2S and starts the task. Call after allocate(). Returns false on
  // failure (the rest of the firmware keeps running without audio).
  bool start();

  int bandCount() const { return bank_.bandCount(); }

  // For the LED task only: copies the newest frame (and bandCount() levels) if
  // one has arrived since the last call. Never blocks for long.
  bool latest(AudioFrame* out, float* levels) { return mailbox_.readIfNew(&lastTaken_, out, levels); }

  // For everyone else: copies the most recent frame (frame.sequence tells
  // whether it's new); levels may be nullptr. Returns false before the first frame.
  bool snapshot(AudioFrame* out, float* levels = nullptr) const { return mailbox_.read(out, levels); }

  const FilterBank& filterBank() const { return bank_; }

  // Asks the audio task to learn the room's noise for kLearnNoiseSeconds (for a
  // new steady noise such as a fan). Safe to call from any core.
  void requestNoiseLearning() { learnRequested_.store(true); }

  // Pauses the analysis (about 43% of core 0), e.g. during a firmware update,
  // when writing flash also takes much of core 0. The microphone keeps being
  // read so its buffers stay drained; no frames are published meanwhile.
  // Safe to call from any core.
  void setPaused(bool paused) { paused_.store(paused); }

  // Count of I2S reads that failed or came back short (should stay 0).
  uint32_t readErrors() const { return readErrors_.load(); }

 private:
  static void taskEntry(void* self);
  void run();
  bool setupI2s();

  i2s_chan_handle_t rx_ = nullptr;

  FilterBank bank_;
  LevelTracker tracker_;
  AudioFrame frame_;   // owned by the audio task while building
  LevelsMailbox mailbox_;
  uint32_t lastTaken_ = 0;   // LED task: sequence of the last frame it took

  int32_t raw_[config::kAudioBlockSize];
  float samples_[config::kAudioBlockSize];
  std::atomic<uint32_t> readErrors_{0};   // written on core 0, read on core 1
  std::atomic<bool> learnRequested_{false};   // set on core 1, taken on core 0
  std::atomic<bool> paused_{false};           // set on core 1, read on core 0
};
