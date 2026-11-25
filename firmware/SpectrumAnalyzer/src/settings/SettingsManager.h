// Owns the current settings and the presets, and keeps them in flash.
//
// - Changes to the current settings are saved a few seconds after the last
//   change (kSaveDelayMillis), so dragging a brightness slider doesn't wear the
//   flash with dozens of writes.
// - Preset saves and deletes are written immediately (they are deliberate actions).
// - Every change bumps revision(), so other parts can tell when to react.
//
// Not thread-safe: call it from the main loop only. Network code (later steps)
// hands requests to the main loop rather than calling in directly.
// Plain C++ apart from the SettingsStore it is given, so it is unit-tested on a PC.
#pragma once

#include <cstdint>

#include "LightSettings.h"
#include "PresetName.h"
#include "SettingsStore.h"

class SettingsManager {
 public:
  static constexpr int kPresetCount = 8;
  static constexpr uint32_t kSaveDelayMillis = 5000;

  explicit SettingsManager(SettingsStore& store) : store_(store) {}

  // Loads the current settings and presets from the store. Anything missing or
  // unreadable falls back to defaults (current) or an empty slot (preset).
  void begin();

  const LightSettings& current() const { return current_; }
  uint32_t revision() const { return revision_; }

  // Replaces the current settings. Saved after kSaveDelayMillis without further changes.
  void update(const LightSettings& next, uint32_t nowMillis);

  // Convenience setters built on update().
  void setMode(Mode mode, uint32_t nowMillis);
  void setBrightness(uint8_t brightness, uint32_t nowMillis);
  void setOn(bool on, uint32_t nowMillis);
  void setColourA(const Rgb& colour, uint32_t nowMillis);
  void setColourB(const Rgb& colour, uint32_t nowMillis);
  void setSpectrumColour(SpectrumColour sc, uint32_t nowMillis);

  // Writes a pending change if the save delay has passed. Call every loop.
  void tick(uint32_t nowMillis);

  // Writes a pending change now (for example just before a restart).
  void flush();
  bool hasUnsavedChanges() const { return dirty_; }

  // Presets: slot is 0..kPresetCount-1. A preset stores mode, colours,
  // brightness and a name; recalling one keeps the current on/off state.
  bool presetUsed(int slot) const;
  const LightSettings* preset(int slot) const;   // nullptr if empty or out of range
  // The slot's name; "Preset N" if never named (or the slot is empty).
  const char* presetName(int slot) const;
  // Saves the current settings into the slot. name: nullptr keeps the slot's
  // existing name; otherwise it is tidied (see tidyPresetName).
  bool savePreset(int slot, const char* name = nullptr);
  bool renamePreset(int slot, const char* name);   // only a saved preset
  bool recallPreset(int slot, uint32_t nowMillis);
  bool deletePreset(int slot);

  // Erases the current settings, presets and names, and returns to defaults.
  // (Other things in the same store, such as the strip setup, are kept.)
  void factoryReset();

 private:
  static void presetKey(int slot, char* buf);   // "p0".."p7"
  static void nameKey(int slot, char* buf);     // "n0".."n7"
  bool writeName(int slot, const char* tidied);
  bool validSlot(int slot) const { return slot >= 0 && slot < kPresetCount; }
  bool writeCurrent();

  SettingsStore& store_;
  LightSettings current_;
  LightSettings lastSaved_;
  LightSettings presets_[kPresetCount];
  bool presetUsed_[kPresetCount] = {};
  char names_[kPresetCount][kPresetNameMaxBytes + 1] = {};
  bool dirty_ = false;
  uint32_t lastChangeMillis_ = 0;
  uint32_t revision_ = 0;
};
