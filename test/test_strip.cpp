// Unit tests for the strip setup (LEDs in use, bass end): the model, keeping it
// in flash, the web API, and the analysis and modes with fewer LEDs.

#include <cmath>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "audio/FilterBank.h"
#include "audio/LevelTracker.h"
#include "config/Config.h"
#include "modes/ModeManager.h"
#include "settings/SettingsManager.h"
#include "settings/StripSetupManager.h"
#include "web/WebApi.h"

namespace {

class Args : public FormArgs {
 public:
  Args(std::initializer_list<std::pair<const std::string, std::string>> v) : values_(v) {}
  const char* get(const char* name) const override {
    auto it = values_.find(name);
    return it == values_.end() ? nullptr : it->second.c_str();
  }

 private:
  std::map<std::string, std::string> values_;
};

StripSetup setup(int leds, bool right = false) {
  StripSetup s;
  s.ledCount = static_cast<uint16_t>(leds);
  s.bassOnRight = right;
  return s;
}

}  // namespace

// ---- The model and flash -------------------------------------------------------------------

TEST(strip_defaults_and_limits) {
  const StripSetup s;
  CHECK_EQ(s.ledCount, 150);
  CHECK(!s.bassOnRight);
  CHECK(strip_setup::validate(setup(1)).empty());
  CHECK(strip_setup::validate(setup(150)).empty());
  CHECK(!strip_setup::validate(setup(0)).empty());
  CHECK(!strip_setup::validate(setup(151)).empty());
}

TEST(strip_flash_format_round_trips_and_rejects_junk) {
  uint8_t buf[strip_setup::kEncodedSize];
  strip_setup::encode(setup(87, true), buf);
  StripSetup back;
  CHECK(strip_setup::decode(buf, sizeof(buf), &back));
  CHECK(back == setup(87, true));
  StripSetup untouched;
  CHECK(!strip_setup::decode(buf, sizeof(buf) - 1, &untouched));   // wrong length
  buf[0] = 99;                                                       // unknown format
  CHECK(!strip_setup::decode(buf, sizeof(buf), &untouched));
  strip_setup::encode(setup(87), buf);
  buf[1] = 200;                                                      // 200 LEDs: over the limit
  CHECK(!strip_setup::decode(buf, sizeof(buf), &untouched));
  CHECK(untouched == StripSetup{});
}

TEST(strip_manager_saves_changes_and_says_when_to_restart) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  CHECK(m.chosen() == StripSetup{});
  bool restart = true;
  CHECK(m.change(setup(150, true), &restart).empty());
  CHECK(!restart);                       // which end is bass: at once
  CHECK(m.running().bassOnRight);
  CHECK(m.change(setup(120, true), &restart).empty());
  CHECK(restart);                        // a new count needs a restart
  CHECK_EQ(m.running().ledCount, 150);   // still using 150 until then
  StripSetupManager after(store);        // after the restart
  after.begin();
  CHECK(after.running() == setup(120, true));
}

TEST(strip_manager_refuses_bad_counts) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  const int writes = store.writes;
  bool restart = false;
  CHECK(!m.change(setup(151), &restart).empty());
  CHECK(m.chosen() == StripSetup{});
  CHECK_EQ(store.writes, writes);
}

TEST(strip_manager_ignores_the_trial_builds_keys) {
  // The step 10 trial builds saved a 12-byte setup (with frequencies) under "strip".
  FakeSettingsStore store;
  const uint8_t old[12] = {1, 100, 0, 1, 0, 0, 0x20, 0x42, 0, 0x60, 0x6A, 0x46};
  store.write("strip", old, sizeof(old));
  store.write("stripok", old, sizeof(old));
  StripSetupManager m(store);
  m.begin();
  CHECK(m.chosen() == StripSetup{});     // defaults: the old format isn't read
  uint8_t buf[16];
  CHECK_EQ(store.read("strip", buf, sizeof(buf)), 0u);   // and tidied away
  CHECK_EQ(store.read("stripok", buf, sizeof(buf)), 0u);
}

TEST(strip_setup_survives_reset_settings) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  StripSetupManager strip(store);
  strip.begin();
  bool restart;
  strip.change(setup(99, true), &restart);
  settings.savePreset(0, "Keep me?");
  settings.factoryReset();
  StripSetupManager after(store);
  after.begin();
  CHECK(after.chosen() == setup(99, true));   // kept, like Wi-Fi
  SettingsManager settingsAfter(store);
  settingsAfter.begin();
  CHECK(!settingsAfter.presetUsed(0));        // presets did go
}

// ---- Web API -------------------------------------------------------------------------------------

TEST(strip_web_api) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  bool restart = true;
  CHECK(web_api::applyStrip(Args{{"bassEnd", "right"}}, m, &restart).empty());
  CHECK(!restart && m.chosen().bassOnRight);
  CHECK(web_api::applyStrip(Args{{"ledCount", "144"}}, m, &restart).empty());
  CHECK(restart && m.chosen() == setup(144, true));
  const StripSetup before = m.chosen();
  for (const auto& bad : std::vector<std::pair<std::string, std::string>>{
           {"ledCount", "0"}, {"ledCount", "151"}, {"ledCount", "12a"}, {"ledCount", ""}, {"bassEnd", "up"}}) {
    // Each bad field with a valid one alongside: nothing at all changes.
    const std::string err = web_api::applyStrip(
        Args{{bad.first, bad.second}, {bad.first == "bassEnd" ? "ledCount" : "bassEnd",
                                       bad.first == "bassEnd" ? "50" : "left"}},
        m, &restart);
    if (err.empty()) std::printf("      accepted bad %s=%s\n", bad.first.c_str(), bad.second.c_str());
    CHECK(!err.empty());
    CHECK(m.chosen() == before);
  }
}

TEST(strip_state_json) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  DeviceStatus st;
  st.hostname = "spectrum";
  st.stripChosen = setup(144, true);
  st.stripRunning = StripSetup{};
  st.heapFree = 76000;
  st.heapLowest = 70000;
  const std::string json = web_api::stateJson(settings, st);
  std::ofstream("build/json/state_strip.json") << json;
  for (const char* field : {"\"ledCount\":144", "\"bassEnd\":\"right\"", "\"runningLedCount\":150",
                            "\"minLeds\":1", "\"maxLeds\":150", "\"memory\":{\"free\":76000,\"lowest\":70000}"}) {
    if (json.find(field) == std::string::npos) std::printf("      missing: %s\n", field);
    CHECK(json.find(field) != std::string::npos);
  }
}

// ---- Fewer LEDs ------------------------------------------------------------------------------------

TEST(filterbank_and_tracker_with_fewer_leds) {
  for (int n : {1, 2, 60, 150}) {
    FilterBank bank;
    CHECK(bank.begin(n, config::kSampleRate, config::kBandLowHz, config::kBandHighHz, config::kBandQScale,
                     config::kMinBandwidthHz));
    CHECK_EQ(bank.bandCount(), n);
    if (n > 1) {
      CHECK(std::fabs(bank.centreHz(0) - config::kBandLowHz) < 0.01f);
      CHECK(std::fabs(bank.centreHz(n - 1) - config::kBandHighHz) < 2.0f);
    } else {
      CHECK(std::fabs(bank.centreHz(0) - std::sqrt(config::kBandLowHz * config::kBandHighHz)) < 1.0f);
    }
    LevelTracker tracker;
    float centres[FilterBank::kMaxBands];
    for (int i = 0; i < n; ++i) centres[i] = bank.centreHz(i);
    tracker.begin(n, centres);
    CHECK_EQ(tracker.bandCount(), n);
    float power[FilterBank::kMaxBands];
    for (int i = 0; i < n; ++i) power[i] = 1e-11f;
    AudioFrame frame;
    for (int f = 0; f < 300; ++f) tracker.update(power, 0.0107f, &frame);
    power[n / 2] = 1e-5f;   // a loud band
    for (int f = 0; f < 30; ++f) tracker.update(power, 0.0107f, &frame);
    CHECK(frame.level[n / 2] > 0.9f);
  }
  FilterBank bank;
  CHECK(!bank.begin(0, config::kSampleRate, 40, 15000, 1, 5));
  CHECK(!bank.begin(151, config::kSampleRate, 40, 15000, 1, 5));
}

TEST(filterbank_tone_lands_on_its_band_with_fewer_leds) {
  const int n = 60, target = 30;
  FilterBank bank;
  CHECK(bank.begin(n, config::kSampleRate, config::kBandLowHz, config::kBandHighHz, config::kBandQScale,
                   config::kMinBandwidthHz));
  const double hz = bank.centreHz(target);
  float block[FilterBank::kBlock];
  double phase = 0;
  for (int b = 0; b < 200; ++b) {
    for (int s = 0; s < FilterBank::kBlock; ++s) {
      block[s] = static_cast<float>(0.5 * std::sin(phase));
      phase += 2 * M_PI * hz / config::kSampleRate;
    }
    bank.process(block);
  }
  int loudest = 0;
  for (int i = 1; i < n; ++i)
    if (bank.power(i) > bank.power(loudest)) loudest = i;
  CHECK_EQ(loudest, target);
  CHECK(bank.power(target - 1) < 0.25 * bank.power(target) && bank.power(target + 1) < 0.25 * bank.power(target));
}

TEST(modes_render_fewer_leds) {
  for (int n : {1, 7, 100}) {
    ModeManager m;
    std::vector<Rgb> out(n);
    AudioFrame audio;
    audio.silent = false;
    for (int i = 0; i < n; ++i) audio.level[i] = 1.0f;
    for (Mode mode : {Mode::SpectrumSnappy, Mode::SpectrumSmooth, Mode::Solid, Mode::Gradient, Mode::Rainbow,
                      Mode::Twinkle}) {
      LightSettings s;
      s.mode = mode;
      for (uint32_t t = 0; t < 200; t += 10) m.render(s, &audio, t, out.data(), n);
    }
    LightSettings spectrum;
    spectrum.mode = Mode::SpectrumSnappy;
    m.render(spectrum, &audio, 300, out.data(), n);
    CHECK(out[n - 1] != Rgb{});   // last LED in use is lit too
  }
}
