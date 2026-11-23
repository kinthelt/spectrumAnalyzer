// SettingsStore backed by the ESP32's non-volatile storage (Preferences library).
#pragma once

#include <Preferences.h>

#include "SettingsStore.h"

class NvsSettingsStore : public SettingsStore {
 public:
  // Opens the namespace. Call once before anything else.
  bool begin();

  size_t read(const char* key, uint8_t* buf, size_t capacity) override;
  bool write(const char* key, const uint8_t* data, size_t length) override;
  void remove(const char* key) override;
  void eraseAll() override;

 private:
  static constexpr const char* kNamespace = "spectrum";
  Preferences prefs_;
};
