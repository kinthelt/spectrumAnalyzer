#include "SettingsManager.h"

#include "SettingsCodec.h"

#include <cstring>

namespace {
constexpr const char* kCurrentKey = "cur";
}  // namespace

void SettingsManager::presetKey(int slot, char* buf) {
  buf[0] = 'p';
  buf[1] = static_cast<char>('0' + slot);
  buf[2] = '\0';
}

void SettingsManager::nameKey(int slot, char* buf) {
  buf[0] = 'n';
  buf[1] = static_cast<char>('0' + slot);
  buf[2] = '\0';
}

bool SettingsManager::writeName(int slot, const char* tidied) {
  char key[3];
  nameKey(slot, key);
  if (!store_.write(key, reinterpret_cast<const uint8_t*>(tidied), std::strlen(tidied))) return false;
  std::strcpy(names_[slot], tidied);
  return true;
}

void SettingsManager::begin() {
  uint8_t buf[settings_codec::kEncodedSize];

  LightSettings loaded;
  const size_t n = store_.read(kCurrentKey, buf, sizeof(buf));
  if (n > 0 && settings_codec::decode(buf, n, &loaded)) {
    current_ = loaded;
  } else {
    current_ = LightSettings{};
  }
  lastSaved_ = current_;
  dirty_ = false;

  for (int slot = 0; slot < kPresetCount; ++slot) {
    char key[3];
    presetKey(slot, key);
    const size_t len = store_.read(key, buf, sizeof(buf));
    presetUsed_[slot] = len > 0 && settings_codec::decode(buf, len, &presets_[slot]);

    // Names were added later: a missing name is simply the default.
    char nkey[3];
    nameKey(slot, nkey);
    char raw[kPresetNameMaxBytes + 1] = {};
    const size_t nlen = store_.read(nkey, reinterpret_cast<uint8_t*>(raw), kPresetNameMaxBytes);
    raw[nlen] = '\0';
    tidyPresetName(raw, slot, names_[slot]);
  }
  ++revision_;
}

void SettingsManager::update(const LightSettings& next, uint32_t nowMillis) {
  if (next == current_) return;
  current_ = next;
  dirty_ = current_ != lastSaved_;
  lastChangeMillis_ = nowMillis;
  ++revision_;
}

void SettingsManager::setMode(Mode mode, uint32_t nowMillis) {
  LightSettings s = current_;
  s.mode = mode;
  update(s, nowMillis);
}

void SettingsManager::setBrightness(uint8_t brightness, uint32_t nowMillis) {
  LightSettings s = current_;
  s.brightness = brightness;
  update(s, nowMillis);
}

void SettingsManager::setOn(bool on, uint32_t nowMillis) {
  LightSettings s = current_;
  s.on = on;
  update(s, nowMillis);
}

void SettingsManager::setColourA(const Rgb& colour, uint32_t nowMillis) {
  LightSettings s = current_;
  s.colourA = colour;
  update(s, nowMillis);
}

void SettingsManager::setColourB(const Rgb& colour, uint32_t nowMillis) {
  LightSettings s = current_;
  s.colourB = colour;
  update(s, nowMillis);
}

void SettingsManager::setSpectrumColour(SpectrumColour sc, uint32_t nowMillis) {
  LightSettings s = current_;
  s.spectrumColour = sc;
  update(s, nowMillis);
}

void SettingsManager::tick(uint32_t nowMillis) {
  if (dirty_ && nowMillis - lastChangeMillis_ >= kSaveDelayMillis) writeCurrent();
}

void SettingsManager::flush() {
  if (dirty_) writeCurrent();
}

bool SettingsManager::writeCurrent() {
  uint8_t buf[settings_codec::kEncodedSize];
  settings_codec::encode(current_, buf);
  if (!store_.write(kCurrentKey, buf, sizeof(buf))) return false;
  lastSaved_ = current_;
  dirty_ = false;
  return true;
}

bool SettingsManager::presetUsed(int slot) const { return validSlot(slot) && presetUsed_[slot]; }

const LightSettings* SettingsManager::preset(int slot) const {
  return presetUsed(slot) ? &presets_[slot] : nullptr;
}

const char* SettingsManager::presetName(int slot) const {
  static const char kInvalid[] = "";
  return validSlot(slot) ? names_[slot] : kInvalid;
}

bool SettingsManager::renamePreset(int slot, const char* name) {
  if (!presetUsed(slot)) return false;
  char tidied[kPresetNameMaxBytes + 1];
  tidyPresetName(name, slot, tidied);
  if (!writeName(slot, tidied)) return false;
  ++revision_;
  return true;
}

bool SettingsManager::savePreset(int slot, const char* name) {
  if (!validSlot(slot)) return false;
  LightSettings p = current_;
  p.on = true;   // on/off isn't part of a preset
  uint8_t buf[settings_codec::kEncodedSize];
  settings_codec::encode(p, buf);
  char key[3];
  presetKey(slot, key);
  if (!store_.write(key, buf, sizeof(buf))) return false;
  presets_[slot] = p;
  presetUsed_[slot] = true;
  if (name != nullptr) {
    char tidied[kPresetNameMaxBytes + 1];
    tidyPresetName(name, slot, tidied);
    writeName(slot, tidied);
  }
  ++revision_;
  return true;
}

bool SettingsManager::recallPreset(int slot, uint32_t nowMillis) {
  if (!presetUsed(slot)) return false;
  LightSettings next = presets_[slot];
  next.on = current_.on;
  update(next, nowMillis);
  return true;
}

bool SettingsManager::deletePreset(int slot) {
  if (!validSlot(slot)) return false;
  char key[3];
  presetKey(slot, key);
  store_.remove(key);
  nameKey(slot, key);
  store_.remove(key);
  defaultPresetName(slot, names_[slot]);
  presetUsed_[slot] = false;
  ++revision_;
  return true;
}

void SettingsManager::factoryReset() {
  store_.eraseAll();
  current_ = LightSettings{};
  lastSaved_ = current_;
  dirty_ = false;
  for (int slot = 0; slot < kPresetCount; ++slot) {
    presetUsed_[slot] = false;
    defaultPresetName(slot, names_[slot]);
  }
  ++revision_;
}
