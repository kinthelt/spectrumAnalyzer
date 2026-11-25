// Unit tests for the strip setup (length, bass end, frequency range): the model,
// keeping it in flash, start-up fallback, the web API, and the audio analysis and
// modes at lengths and ranges other than the defaults.

#include <cmath>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "audio/FilterBank.h"
#include "audio/LevelTracker.h"
#include "audio/LevelsMailbox.h"
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

StripSetup setup(int leds, float lo, float hi, bool right = false) {
  StripSetup s;
  s.ledCount = static_cast<uint16_t>(leds);
  s.lowHz = lo;
  s.highHz = hi;
  s.bassOnRight = right;
  return s;
}

bool designs(int leds, float lo, float hi) {
  FilterBank bank;
  return bank.begin(leds, config::kSampleRate, lo, hi, config::kBandQScale, config::kMinBandwidthHz);
}

}  // namespace

// ---- The model -----------------------------------------------------------------------------

TEST(strip_defaults_are_the_bench_setup) {
  const StripSetup s;
  CHECK_EQ(s.ledCount, 150);
  CHECK(!s.bassOnRight);
  CHECK(s.lowHz == 40.0f && s.highHz == 15000.0f);
  CHECK(strip_setup::validate(s).empty());
}

TEST(strip_limits) {
  CHECK(strip_setup::validate(setup(1, 20, 20000)).empty());      // the extremes are fine
  CHECK(strip_setup::validate(setup(1000, 20, 40)).empty());      // exactly one octave
  CHECK(!strip_setup::validate(setup(0, 40, 15000)).empty());
  CHECK(!strip_setup::validate(setup(1001, 40, 15000)).empty());
  CHECK(!strip_setup::validate(setup(150, 19, 15000)).empty());
  CHECK(!strip_setup::validate(setup(150, 40, 20001)).empty());
  CHECK(strip_setup::validate(setup(150, 1000, 1999)).find("octave") != std::string::npos);
  CHECK(!strip_setup::validate(setup(150, std::nanf(""), 15000)).empty());   // never accepts NaN
}

TEST(strip_restart_needed_only_for_length_and_range) {
  const StripSetup a;
  StripSetup b = a;
  b.bassOnRight = true;
  CHECK(!a.needsRestartFor(b));   // which end is bass: at once
  b.ledCount = 120;
  CHECK(a.needsRestartFor(b));
  b = a;
  b.highHz = 12000;
  CHECK(a.needsRestartFor(b));
}

TEST(strip_flash_format_round_trips_and_rejects_junk) {
  const StripSetup s = setup(87, 31.5f, 12345.0f, true);
  uint8_t buf[strip_setup::kEncodedSize];
  strip_setup::encode(s, buf);
  StripSetup back;
  CHECK(strip_setup::decode(buf, sizeof(buf), &back));
  CHECK(back == s);

  StripSetup untouched;
  CHECK(!strip_setup::decode(buf, sizeof(buf) - 1, &untouched));   // wrong length
  uint8_t bad[strip_setup::kEncodedSize];
  std::copy(buf, buf + sizeof(buf), bad);
  bad[0] = 99;                                                       // unknown format
  CHECK(!strip_setup::decode(bad, sizeof(bad), &untouched));
  std::copy(buf, buf + sizeof(buf), bad);
  bad[1] = 0;
  bad[2] = 0;                                                        // 0 LEDs: invalid
  CHECK(!strip_setup::decode(bad, sizeof(bad), &untouched));
  CHECK(untouched == StripSetup{});
}

// ---- Keeping it, and start-up ----------------------------------------------------------------

TEST(strip_manager_saves_changes_and_says_when_to_restart) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  CHECK(m.chosen() == StripSetup{});
  m.setRunning(m.chosen());

  bool restart = true;
  StripSetup flipped = m.chosen();
  flipped.bassOnRight = true;
  CHECK(m.change(flipped, &restart).empty());
  CHECK(!restart);
  CHECK(m.running().bassOnRight);   // applies at once

  CHECK(m.change(setup(120, 40, 15000, true), &restart).empty());
  CHECK(restart);
  CHECK_EQ(m.running().ledCount, 150);   // still running the old length until the restart

  // Survives a restart.
  StripSetupManager after(store);
  after.begin();
  CHECK(after.chosen() == setup(120, 40, 15000, true));
}

TEST(strip_manager_refuses_bad_setups_and_changes_nothing) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  const int writes = store.writes;
  bool restart = false;
  CHECK(!m.change(setup(5000, 40, 15000), &restart).empty());
  CHECK(m.chosen() == StripSetup{});
  CHECK_EQ(store.writes, writes);
}

TEST(strip_manager_remembers_the_last_setup_that_worked) {
  FakeSettingsStore store;
  {
    StripSetupManager m(store);
    m.begin();
    bool restart;
    m.change(setup(120, 40, 15000), &restart);
    m.setRunning(m.chosen());          // started fine with 120
    m.change(setup(900, 20, 20000), &restart);
  }
  StripSetupManager m(store);           // next start-up: 900 doesn't fit in memory
  m.begin();
  CHECK_EQ(m.chosen().ledCount, 900);
  CHECK_EQ(m.lastWorking().ledCount, 120);
  m.setRunning(m.lastWorking());
  CHECK(m.fellBack());
  bool restart = false;
  CHECK(m.change(setup(140, 40, 15000), &restart).empty());   // fixing it on the page
  CHECK(restart);
}

TEST(strip_setup_survives_reset_settings) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  StripSetupManager strip(store);
  strip.begin();
  bool restart;
  strip.change(setup(99, 30, 16000, true), &restart);
  settings.savePreset(0, "Keep me?");
  settings.factoryReset();
  StripSetupManager after(store);
  after.begin();
  CHECK(after.chosen() == setup(99, 30, 16000, true));   // kept, like Wi-Fi
  SettingsManager settingsAfter(store);
  settingsAfter.begin();
  CHECK(!settingsAfter.presetUsed(0));                     // presets did go
}

// ---- Web API -------------------------------------------------------------------------------------

TEST(strip_web_api) {
  FakeSettingsStore store;
  StripSetupManager m(store);
  m.begin();
  m.setRunning(m.chosen());
  bool restart = true;
  CHECK(web_api::applyStrip(Args{{"bassEnd", "right"}}, m, &restart).empty());
  CHECK(!restart && m.chosen().bassOnRight);
  CHECK(web_api::applyStrip(Args{{"ledCount", "144"}, {"lowHz", "30"}, {"highHz", "16000"}}, m, &restart).empty());
  CHECK(restart);
  CHECK(m.chosen() == setup(144, 30, 16000, true));

  const StripSetup before = m.chosen();
  for (const auto& bad : std::vector<std::pair<std::string, std::string>>{
           {"ledCount", "0"}, {"ledCount", "1001"}, {"ledCount", "12a"}, {"bassEnd", "up"},
           {"lowHz", "10"}, {"highHz", "25000"}, {"lowHz", "4.5"}, {"lowHz", "9000"}}) {
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
  st.stripChosen = setup(144, 30, 16000, true);
  st.stripRunning = StripSetup{};
  st.stripFellBack = true;
  st.heapFree = 76000;
  st.heapLowest = 70000;
  const std::string json = web_api::stateJson(settings, st);
  std::ofstream("build/json/state_strip.json") << json;
  for (const char* field : {"\"ledCount\":144", "\"bassEnd\":\"right\"", "\"lowHz\":30", "\"highHz\":16000",
                            "\"runningLedCount\":150", "\"fellBack\":true", "\"maxLeds\":1000",
                            "\"lowestHz\":20", "\"highestHz\":20000", "\"loadLimitPercent\":65",
                            "\"memory\":{\"free\":76000,\"lowest\":70000}"}) {
    if (json.find(field) == std::string::npos) std::printf("      missing: %s\n", field);
    CHECK(json.find(field) != std::string::npos);
  }
}

// ---- Audio at other lengths and ranges ---------------------------------------------------------------

TEST(filterbank_builds_every_length_and_range_the_page_allows) {
  for (int leds : {1, 2, 3, 30, 150, 300, 1000}) {
    for (const auto& range : std::vector<std::pair<float, float>>{{20, 40}, {20, 20000}, {40, 15000}, {10000, 20000}}) {
      if (!designs(leds, range.first, range.second)) {
        std::printf("      failed: %d LEDs %.0f-%.0f Hz\n", leds, range.first, range.second);
        CHECK(false);
      }
    }
  }
  FilterBank bank;
  bank.begin(150, config::kSampleRate, 20, 20000, config::kBandQScale, config::kMinBandwidthHz);
  CHECK_EQ(bank.stageCount(), 9);   // 20 Hz needs every stage the block allows
  CHECK(std::fabs(bank.centreHz(0) - 20.0f) < 0.01f);
  CHECK(std::fabs(bank.centreHz(149) - 20000.0f) < 2.0f);
}

TEST(filterbank_one_led_sits_in_the_middle_of_the_range) {
  FilterBank bank;
  CHECK(bank.begin(1, config::kSampleRate, 100, 10000, config::kBandQScale, config::kMinBandwidthHz));
  CHECK(std::fabs(bank.centreHz(0) - 1000.0f) < 0.5f);   // geometric middle of 100 Hz-10 kHz
  CHECK(bank.q(0) > 0.05f && bank.q(0) < 0.2f);          // about as wide as the whole range
}

TEST(filterbank_tone_lands_on_its_band_at_other_lengths) {
  // 60 LEDs over 50 Hz-5 kHz: a tone at LED 30's centre lights LED 30 most.
  const int n = 60, target = 30;
  FilterBank bank;
  CHECK(bank.begin(n, config::kSampleRate, 50, 5000, config::kBandQScale, config::kMinBandwidthHz));
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
  // Neighbours clearly lower (they meet at -3 dB halfway, so a band away is well down).
  CHECK(bank.power(target - 1) < 0.25 * bank.power(target) && bank.power(target + 1) < 0.25 * bank.power(target));
}

TEST(filterbank_cost_grows_with_leds) {
  // The analysis cost is proportional to band-samples per second. On the board,
  // 150 LEDs over 40 Hz-15 kHz measured 43-45% of core 0; this estimates others.
  auto cost = [](int leds, float lo, float hi) {
    FilterBank bank;
    bank.begin(leds, config::kSampleRate, lo, hi, config::kBandQScale, config::kMinBandwidthHz);
    return bank.bandSamplesPerSecond();
  };
  const double base = cost(150, 40, 15000);
  const double at300 = cost(300, 40, 15000), at75 = cost(75, 40, 15000), wide = cost(150, 20, 20000);
  std::printf("      estimated core 0 use: 75 LEDs %.0f%%, 150 LEDs 44%%, 300 LEDs %.0f%%, 150 LEDs 20 Hz-20 kHz %.0f%%\n",
              44 * at75 / base, 44 * at300 / base, 44 * wide / base);
  CHECK(at300 > 1.8 * base && at300 < 2.2 * base);
}

TEST(tracker_works_at_other_lengths) {
  for (int n : {1, 7, 300}) {
    FilterBank bank;
    bank.begin(n, config::kSampleRate, 40, 15000, config::kBandQScale, config::kMinBandwidthHz);
    LevelTracker tracker;
    CHECK(tracker.begin(n, bank.centres()));
    CHECK_EQ(tracker.bandCount(), n);
    std::vector<float> power(n, 1e-11f);
    AudioFrame frame;
    for (int f = 0; f < 300; ++f) tracker.update(power.data(), 0.0107f, &frame);
    power[n / 2] = 1e-5f;   // a loud band
    for (int f = 0; f < 30; ++f) tracker.update(power.data(), 0.0107f, &frame);
    CHECK(tracker.level(n / 2) > 0.9f);
  }
}

// ---- Handing levels between tasks ------------------------------------------------------------

TEST(levels_mailbox_newest_wins_and_reads_are_whole) {
  LevelsMailbox box;
  CHECK(box.begin(5));
  AudioFrame f;
  float out[5];
  uint32_t seen = 0;
  CHECK(!box.read(&f, out));                 // nothing yet
  CHECK(!box.readIfNew(&seen, &f, out));

  AudioFrame in;
  const float a[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
  in.sequence = 1;
  box.publish(in, a);
  CHECK(box.readIfNew(&seen, &f, out));
  CHECK(seen == 1 && out[4] == 0.5f);
  CHECK(!box.readIfNew(&seen, &f, out));     // nothing new since
  CHECK(box.read(&f, nullptr));              // summary only, any time

  // A writer and a reader racing: every read is one whole frame (all values equal).
  const float ones[5] = {1, 1, 1, 1, 1};
  box.publish(in, ones);
  std::thread writer([&] {
    float v[5];
    for (uint32_t k = 2; k < 20000; ++k) {
      for (float& x : v) x = static_cast<float>(k);
      AudioFrame g;
      g.sequence = k;
      box.publish(g, v);
    }
  });
  int torn = 0;
  for (int r = 0; r < 20000; ++r) {
    if (box.read(&f, out)) {
      for (float x : out) torn += x != out[0];
    }
  }
  writer.join();
  CHECK_EQ(torn, 0);
}

// ---- Modes at other lengths ---------------------------------------------------------------------------

TEST(modes_render_any_length) {
  for (int n : {1, 7, 1000}) {
    ModeManager m;
    CHECK(m.allocate(n));
    std::vector<Rgb> out(n);
    std::vector<float> levels(n, 1.0f);
    AudioFrame audio;
    audio.silent = false;
    for (Mode mode : {Mode::SpectrumSnappy, Mode::SpectrumSmooth, Mode::Solid, Mode::Gradient, Mode::Rainbow,
                      Mode::Twinkle}) {
      LightSettings s;
      s.mode = mode;
      for (uint32_t t = 0; t < 200; t += 10) m.render(s, &audio, levels.data(), t, out.data(), n);
    }
    LightSettings spectrum;
    spectrum.mode = Mode::SpectrumSnappy;
    m.render(spectrum, &audio, levels.data(), 300, out.data(), n);
    CHECK(out[n - 1] != Rgb{});   // last LED lit too
  }
}
