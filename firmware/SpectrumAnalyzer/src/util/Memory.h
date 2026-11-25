// Memory for things sized at start-up (per-LED arrays: the strip length is a setting).
//
// On the board these go in the 8 MB PSRAM, falling back to internal RAM only if
// there's no PSRAM. Internal RAM is kept for what can't use PSRAM: Wi-Fi, and
// Matter with the encryption it uses. With Matter running it was down to ~2 KB
// at its lowest on the bench (step 10), and the board crashed when it ran out.
// The audio code works on local copies of its filter state, so PSRAM's slower
// access costs little. On a PC: plain calloc. Memory comes back zeroed.
#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace memory {

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
