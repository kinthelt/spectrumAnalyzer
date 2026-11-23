// Microphone input and analysis, running in its own task on core 0.
//
// The task reads 512-sample blocks from the ICS-43434 over I2S, runs the filter
// bank and level tracker, and publishes an AudioFrame through a one-slot queue
// (newest frame wins). The LED code on core 1 picks frames up with latest().
#pragma once

#include <atomic>

#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../config/Config.h"
#include "AudioFrame.h"
#include "FilterBank.h"
#include "LevelTracker.h"

class AudioInput {
 public:
  // Sets up I2S and the analysis, and starts the task. Returns false on failure
  // (the rest of the firmware keeps running without audio).
  bool begin();

  // Copies the newest frame into out if one has arrived since the last call.
  // Never blocks.
  bool latest(AudioFrame* out);

  const FilterBank& filterBank() const { return bank_; }

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

  int32_t raw_[config::kAudioBlockSize];
  float samples_[config::kAudioBlockSize];
  std::atomic<uint32_t> readErrors_{0};   // written on core 0, read on core 1
};
