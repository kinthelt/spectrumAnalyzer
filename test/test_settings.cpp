// Unit tests for src/settings: colour parsing, the flash codec, and SettingsManager.

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "settings/LightSettings.h"
#include "settings/SettingsCodec.h"
#include "settings/SettingsManager.h"

namespace {

LightSettings sample() {
  LightSettings s;
  s.mode = Mode::Gradient;
  s.spectrumColour = SpectrumColour::Fixed;
  s.colourA = Rgb{1, 2, 3};
  s.colourB = Rgb{250, 251, 252};
  s.brightness = 200;
  s.on = false;
  return s;
}

}  // namespace

// ---- Names and colours --------------------------------------------------------

TEST(mode_names_round_trip) {
  for (uint8_t i = 0; i < kModeCount; ++i) {
    Mode parsed;
    CHECK(modeFromName(modeName(static_cast<Mode>(i)), &parsed));
    CHECK_EQ(static_cast<int>(parsed), i);
  }
  Mode unused;
  CHECK(!modeFromName("disco", &unused));
}

TEST(hex_colour_parsing) {
  Rgb c;
  CHECK(parseHexColour("FF8000", &c));
  CHECK(c == (Rgb{255, 128, 0}));
  CHECK(parseHexColour("#0a0B0c", &c));
  CHECK(c == (Rgb{10, 11, 12}));
  CHECK(!parseHexColour("FF800", &c));     // too short
  CHECK(!parseHexColour("FF80000", &c));   // too long
  CHECK(!parseHexColour("GG0000", &c));    // not hex

  char buf[7];
  formatHexColour(Rgb{255, 128, 0}, buf);
  CHECK(std::string(buf) == "FF8000");
}

// ---- Codec ----------------------------------------------------------------------

TEST(codec_round_trip) {
  uint8_t buf[settings_codec::kEncodedSize];
  settings_codec::encode(sample(), buf);
  LightSettings decoded;
  CHECK(settings_codec::decode(buf, sizeof(buf), &decoded));
  CHECK(decoded == sample());
}

TEST(codec_rejects_bad_data) {
  uint8_t buf[settings_codec::kEncodedSize];
  LightSettings out = sample();

  settings_codec::encode(LightSettings{}, buf);
  CHECK(!settings_codec::decode(buf, sizeof(buf) - 1, &out));   // wrong length

  buf[0] = settings_codec::kVersion + 1;                         // unknown version
  CHECK(!settings_codec::decode(buf, sizeof(buf), &out));

  settings_codec::encode(LightSettings{}, buf);
  buf[1] = kModeCount;                                            // mode out of range
  CHECK(!settings_codec::decode(buf, sizeof(buf), &out));

  settings_codec::encode(LightSettings{}, buf);
  buf[3] = 2;                                                     // on flag not 0/1
  CHECK(!settings_codec::decode(buf, sizeof(buf), &out));

  CHECK(out == sample());   // untouched after every failure
}

// ---- SettingsManager -----------------------------------------------------------------

TEST(empty_store_gives_defaults) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  CHECK(m.current() == LightSettings{});
  for (int i = 0; i < SettingsManager::kPresetCount; ++i) CHECK(!m.presetUsed(i));
  CHECK_EQ(store.writes, 0);
}

TEST(corrupt_current_falls_back_to_defaults) {
  FakeSettingsStore store;
  store.data["cur"] = {9, 9, 9};
  SettingsManager m(store);
  m.begin();
  CHECK(m.current() == LightSettings{});
}

TEST(changes_are_saved_after_the_delay_only) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();

  m.setBrightness(10, 1000);
  m.tick(1000 + SettingsManager::kSaveDelayMillis - 1);
  CHECK_EQ(store.writes, 0);
  CHECK(m.hasUnsavedChanges());

  m.tick(1000 + SettingsManager::kSaveDelayMillis);
  CHECK_EQ(store.writes, 1);
  CHECK(!m.hasUnsavedChanges());
}

TEST(rapid_changes_cause_one_write) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  for (uint32_t t = 0; t < 3000; t += 50) {   // a slider being dragged for 3 s
    m.setBrightness(static_cast<uint8_t>(t / 50), t);
    m.tick(t);
  }
  m.tick(3000 + SettingsManager::kSaveDelayMillis);
  CHECK_EQ(store.writes, 1);
}

TEST(change_and_change_back_writes_nothing) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  const uint8_t original = m.current().brightness;
  m.setBrightness(original + 1, 0);
  m.setBrightness(original, 10);
  CHECK(!m.hasUnsavedChanges());
  m.tick(100000);
  CHECK_EQ(store.writes, 0);
}

TEST(settings_survive_restart) {
  FakeSettingsStore store;
  {
    SettingsManager m(store);
    m.begin();
    m.update(sample(), 0);
    m.flush();
  }
  SettingsManager restarted(store);
  restarted.begin();
  CHECK(restarted.current() == sample());
}

TEST(save_delay_works_across_millis_wraparound) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  const uint32_t nearWrap = 0xFFFFFFFFu - 1000;
  m.setBrightness(5, nearWrap);
  m.tick(nearWrap + 1000);   // 1 s later, still before the delay
  CHECK_EQ(store.writes, 0);
  m.tick(nearWrap + SettingsManager::kSaveDelayMillis);   // wraps past zero
  CHECK_EQ(store.writes, 1);
}

TEST(revision_changes_only_on_real_change) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  const uint32_t r0 = m.revision();
  m.setBrightness(m.current().brightness, 0);   // no actual change
  CHECK_EQ(m.revision(), r0);
  m.setMode(Mode::Solid, 0);
  CHECK(m.revision() != r0);
}

TEST(presets_save_recall_delete) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();

  m.update(sample(), 0);
  CHECK(m.savePreset(2));
  CHECK(m.presetUsed(2));
  CHECK(m.preset(2)->on);   // on/off is not part of a preset

  m.update(LightSettings{}, 0);
  CHECK(m.recallPreset(2, 0));
  LightSettings expected = sample();
  expected.on = LightSettings{}.on;   // recall keeps the current on/off state
  CHECK(m.current() == expected);

  CHECK(m.deletePreset(2));
  CHECK(!m.presetUsed(2));
  CHECK(!m.recallPreset(2, 0));
}

TEST(presets_survive_restart) {
  FakeSettingsStore store;
  {
    SettingsManager m(store);
    m.begin();
    m.update(sample(), 0);
    m.savePreset(7);
  }
  SettingsManager restarted(store);
  restarted.begin();
  CHECK(restarted.presetUsed(7));
  CHECK_EQ(restarted.preset(7)->brightness, sample().brightness);
  CHECK(!restarted.presetUsed(0));
}

TEST(preset_slots_out_of_range_are_rejected) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  CHECK(!m.savePreset(-1));
  CHECK(!m.savePreset(SettingsManager::kPresetCount));
  CHECK(m.preset(SettingsManager::kPresetCount) == nullptr);
  CHECK_EQ(store.writes, 0);
}

TEST(failed_write_stays_unsaved_and_retries) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  store.failWrites = true;
  m.setBrightness(1, 0);
  m.tick(SettingsManager::kSaveDelayMillis);
  CHECK(m.hasUnsavedChanges());
  store.failWrites = false;
  m.tick(SettingsManager::kSaveDelayMillis + 10);
  CHECK(!m.hasUnsavedChanges());
}

TEST(factory_reset_clears_everything) {
  FakeSettingsStore store;
  SettingsManager m(store);
  m.begin();
  m.update(sample(), 0);
  m.savePreset(0);
  m.flush();
  m.factoryReset();
  CHECK(m.current() == LightSettings{});
  CHECK(!m.presetUsed(0));
  CHECK(store.data.empty());
}
