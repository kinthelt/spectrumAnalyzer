// Microphone input and analysis, running in its own task on core 0.
//
// The task reads 512-sample blocks from the ICS-43434 over I2S, runs the filter
// bank and level tracker, and publishes each AudioFrame two ways:
//   - a one-slot queue (newest wins) that the LED task consumes with latest();
//   - a shared snapshot that anything else (live view, web page) can read
//     with snapshot() without taking frames away from the LEDs.
#pragma once

#include <atomic>

#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../config/Config.h"
#include "../util/Shared.h"
#include "AudioFrame.h"
#include "FilterBank.h"
#include "LevelTracker.h"

class AudioInput {
 public:
  // Sets up I2S and the analysis (one band per LED in use), and starts the task.
  // Returns false on failure (the rest of the firmware keeps running without audio).
  bool begin(int ledCount);
  int bandCount() const { return bank_.bandCount(); }

  // For the LED task only: copies the newest frame into out if one has arrived
  // since the last call. Never blocks.
  bool latest(AudioFrame* out);

  // For everyone else: copies the most recent frame (frame.sequence tells
  // whether it's new). Returns false before the first frame.
  bool snapshot(AudioFrame* out) const { return snapshot_.get(out) != 0; }

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
  QueueHandle_t queue_ = nullptr;

  FilterBank bank_;
  LevelTracker tracker_;
  AudioFrame frame_;   // owned by the audio task while building
  Shared<AudioFrame> snapshot_;

  int32_t raw_[config::kAudioBlockSize];
  float samples_[config::kAudioBlockSize];
  std::atomic<uint32_t> readErrors_{0};   // written on core 0, read on core 1
  std::atomic<bool> learnRequested_{false};   // set on core 1, taken on core 0
  std::atomic<bool> paused_{false};           // set on core 1, read on core 0
};
