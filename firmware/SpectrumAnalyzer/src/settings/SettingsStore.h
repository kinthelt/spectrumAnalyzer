// Where settings are kept between power cycles. The real implementation uses the
// ESP32's flash (NvsSettingsStore); tests use an in-memory fake.
#pragma once

#include <cstddef>
#include <cstdint>

class SettingsStore {
 public:
  virtual ~SettingsStore() = default;

  // Reads the blob stored under key into buf. Returns the number of bytes read,
  // or 0 if the key doesn't exist or doesn't fit.
  virtual size_t read(const char* key, uint8_t* buf, size_t capacity) = 0;

  // Stores length bytes under key. Returns false on failure.
  virtual bool write(const char* key, const uint8_t* data, size_t length) = 0;

  // Removes key. Missing keys are not an error.
  virtual void remove(const char* key) = 0;

  // Removes everything this store holds.
  virtual void eraseAll() = 0;
};
