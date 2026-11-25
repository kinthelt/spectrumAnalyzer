// Memory for things sized at start-up (per-LED arrays: the strip length is a setting).
//
// On the board: fast internal RAM while enough stays free for Wi-Fi, the web
// page and Matter (which starts after these are allocated, and needs ~100 KB),
// otherwise the 8 MB PSRAM (slower, but plenty). On a PC: plain calloc.
// Memory comes back zeroed.
#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace memory {

// Internal RAM is only used while at least this much would remain free afterwards.
constexpr size_t kInternalReserveBytes = 128 * 1024;

void* allocate(size_t bytes);   // nullptr if there's none anywhere
void release(void* p);

}  // namespace memory

// A fixed-size array allocated with memory::allocate. Movable, not copyable.
template <typename T>
class Buffer {
 public:
  Buffer() = default;
  ~Buffer() { reset(); }
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer(Buffer&& o) noexcept : data_(o.data_), size_(o.size_) {
    o.data_ = nullptr;
    o.size_ = 0;
  }
  Buffer& operator=(Buffer&& o) noexcept {
    if (this != &o) {
      reset();
      data_ = o.data_;
      size_ = o.size_;
      o.data_ = nullptr;
      o.size_ = 0;
    }
    return *this;
  }

  // Replaces the contents with count default-constructed elements. False if out of memory.
  bool allocate(size_t count) {
    reset();
    if (count == 0) return true;
    void* p = memory::allocate(count * sizeof(T));
    if (p == nullptr) return false;
    data_ = static_cast<T*>(p);
    for (size_t i = 0; i < count; ++i) new (&data_[i]) T();
    size_ = count;
    return true;
  }

  void reset() {
    for (size_t i = 0; i < size_; ++i) data_[i].~T();
    memory::release(data_);
    data_ = nullptr;
    size_ = 0;
  }

  T& operator[](size_t i) { return data_[i]; }
  const T& operator[](size_t i) const { return data_[i]; }
  T* data() { return data_; }
  const T* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  T* data_ = nullptr;
  size_t size_ = 0;
};
