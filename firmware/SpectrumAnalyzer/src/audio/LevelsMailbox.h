// Hands the newest audio analysis (summary + one level per LED) from the audio
// task to its readers: the LED task, the live view, the status page. Readers
// copy out under a lock, so no one ever reads half-written levels. Newest wins:
// a reader that falls behind just skips frames. Plain C++ (std::mutex), unit-tested.
#pragma once

#include <cstdint>
#include <mutex>

#include "../util/Memory.h"
#include "AudioFrame.h"

class LevelsMailbox {
 public:
  bool begin(int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    hasFrame_ = false;
    return levels_.allocate(count);
  }
  int count() const { return static_cast<int>(levels_.size()); }

  void publish(const AudioFrame& frame, const float* levels) {
    std::lock_guard<std::mutex> lock(mutex_);
    frame_ = frame;
    for (size_t i = 0; i < levels_.size(); ++i) levels_[i] = levels[i];
    hasFrame_ = true;
  }

  // Copies the newest frame if its sequence differs from *seen, and updates *seen.
  // levels (count() floats) may be nullptr when only the summary is wanted.
  bool readIfNew(uint32_t* seen, AudioFrame* frame, float* levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasFrame_ || frame_.sequence == *seen) return false;
    copyOut(frame, levels);
    *seen = frame_.sequence;
    return true;
  }

  // Copies the newest frame, new or not. False before the first one.
  bool read(AudioFrame* frame, float* levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasFrame_) return false;
    copyOut(frame, levels);
    return true;
  }

 private:
  void copyOut(AudioFrame* frame, float* levels) const {
    *frame = frame_;
    if (levels != nullptr) {
      for (size_t i = 0; i < levels_.size(); ++i) levels[i] = levels_[i];
    }
  }

  mutable std::mutex mutex_;
  AudioFrame frame_;
  Buffer<float> levels_;
  bool hasFrame_ = false;
};
