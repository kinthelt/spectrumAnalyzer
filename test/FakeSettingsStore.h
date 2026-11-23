// In-memory SettingsStore for tests. Counts writes so tests can check flash wear.
#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "settings/SettingsStore.h"

class FakeSettingsStore : public SettingsStore {
 public:
  size_t read(const char* key, uint8_t* buf, size_t capacity) override {
    auto it = data.find(key);
    if (it == data.end() || it->second.size() > capacity) return 0;
    std::memcpy(buf, it->second.data(), it->second.size());
    return it->second.size();
  }

  bool write(const char* key, const uint8_t* bytes, size_t length) override {
    ++writes;
    if (failWrites) return false;
    data[key] = std::vector<uint8_t>(bytes, bytes + length);
    return true;
  }

  void remove(const char* key) override { data.erase(key); }
  void eraseAll() override { data.clear(); }

  std::map<std::string, std::vector<uint8_t>> data;
  int writes = 0;
  bool failWrites = false;
};
