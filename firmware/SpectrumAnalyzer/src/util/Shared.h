// A value shared between tasks on different cores: one side writes copies in,
// the other reads copies out, under a lock. Meant for small structs (settings,
// a snapshot of the latest audio frame); never hand out pointers into it.
// Plain C++ (std::mutex works on the ESP32 and on a PC), so it is unit-tested.
#pragma once

#include <mutex>

template <typename T>
class Shared {
 public:
  void set(const T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
    ++version_;
  }

  // Copies the current value into out and returns its version (0 = never set).
  unsigned get(T* out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    *out = value_;
    return version_;
  }

  unsigned version() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return version_;
  }

 private:
  mutable std::mutex mutex_;
  T value_{};
  unsigned version_ = 0;
};
