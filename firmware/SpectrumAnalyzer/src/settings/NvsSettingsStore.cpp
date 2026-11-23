#include "NvsSettingsStore.h"

#include "../util/Log.h"

bool NvsSettingsStore::begin() {
  const bool ok = prefs_.begin(kNamespace, /*readOnly=*/false);
  if (!ok) LOG_ERROR("store", "could not open flash namespace '%s'", kNamespace);
  return ok;
}

size_t NvsSettingsStore::read(const char* key, uint8_t* buf, size_t capacity) {
  if (!prefs_.isKey(key)) return 0;
  const size_t length = prefs_.getBytesLength(key);
  if (length == 0 || length > capacity) return 0;
  return prefs_.getBytes(key, buf, length);
}

bool NvsSettingsStore::write(const char* key, const uint8_t* data, size_t length) {
  const bool ok = prefs_.putBytes(key, data, length) == length;
  if (!ok) LOG_ERROR("store", "write of '%s' failed", key);
  return ok;
}

void NvsSettingsStore::remove(const char* key) {
  if (prefs_.isKey(key)) prefs_.remove(key);
}

void NvsSettingsStore::eraseAll() { prefs_.clear(); }
