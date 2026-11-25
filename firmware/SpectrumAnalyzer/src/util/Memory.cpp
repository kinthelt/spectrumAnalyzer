#include "Memory.h"

#include <cstdlib>

#if defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

namespace memory {

void* allocate(size_t bytes) {
#if defined(ARDUINO)
  constexpr uint32_t kInternal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  void* p = nullptr;
  if (heap_caps_get_free_size(kInternal) >= bytes + kInternalReserveBytes) {
    p = heap_caps_calloc(1, bytes, kInternal);
  }
  if (p == nullptr) {
    p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (p == nullptr) p = heap_caps_calloc(1, bytes, kInternal);   // no PSRAM: dip into the reserve
  return p;
#else
  return std::calloc(1, bytes);
#endif
}

void release(void* p) {
#if defined(ARDUINO)
  heap_caps_free(p);
#else
  std::free(p);
#endif
}

}  // namespace memory
