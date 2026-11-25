// Unit tests for src/smarthome/HomeRules: Apple Home / Google Home (Matter) <-> settings.

#include <cstdlib>

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "smarthome/CrashGuard.h"
#include "smarthome/HomeRules.h"

using namespace home;

namespace {
bool near(int a, int b, int tolerance) { return std::abs(a - b) <= tolerance; }
}  // namespace

// ---- Conversions ------------------------------------------------------------------------------

TEST(home_brightness_levels_round_trip_exactly) {
  for (int level = 1; level <= 254; ++level) {
    const uint8_t b = levelToBrightness(static_cast<uint8_t>(level));
    if (brightnessToLevel(b) != level) std::printf("      level %d -> %u -> %u\n", level, b, brightnessToLevel(b));
    CHECK_EQ(brightnessToLevel(b), level);
  }
  CHECK_EQ(levelToBrightness(254), 255);   // Home's 100% is our full
  CHECK_EQ(brightnessToLevel(255), 254);
  CHECK_EQ(brightnessToLevel(0), 1);       // Matter has no level 0
  CHECK_EQ(levelToBrightness(0), 1);
}

TEST(home_hue_saturation_to_colour) {
  CHECK(hueSatToRgb(0, 254) == (Rgb{255, 0, 0}));        // red
  CHECK(hueSatToRgb(254, 254) == (Rgb{255, 0, 0}));      // all the way round: red again
  const Rgb green = hueSatToRgb(85, 254);                // a third of the way: green
  CHECK(green.g == 255 && green.r <= 3 && green.b <= 3);
  const Rgb blue = hueSatToRgb(169, 254);                // two thirds: blue
  CHECK(blue.b == 255 && blue.g <= 3 && blue.r == 0);
  CHECK(hueSatToRgb(123, 0) == (Rgb{255, 255, 255}));    // no saturation: white
}

TEST(home_colour_round_trips_through_hue_saturation) {
  int worst = 0;
  for (int h = 0; h < 254; h += 3) {
    for (int s = 30; s <= 254; s += 32) {
      uint8_t h2, s2;
      const Rgb c = hueSatToRgb(static_cast<uint8_t>(h), static_cast<uint8_t>(s));
      rgbToHueSat(c, &h2, &s2);
      const int dh = std::min(std::abs(h - h2), 254 - std::abs(h - h2));   // around the wheel
      worst = std::max(worst, std::max(dh, std::abs(s - s2)));
    }
  }
  std::printf("      worst hue/saturation round-trip error: %d of 254\n", worst);
  CHECK(worst <= 2);
}

TEST(home_white_temperatures) {
  const Rgb cool = miredsToRgb(153);    // ~6500 K
  const Rgb neutral = miredsToRgb(250); // 4000 K
  const Rgb warm = miredsToRgb(370);    // ~2700 K, a warm bulb
  const Rgb candle = miredsToRgb(500);  // 2000 K
  std::printf("      6500 K %u,%u,%u  4000 K %u,%u,%u  2700 K %u,%u,%u  2000 K %u,%u,%u\n", cool.r, cool.g,
              cool.b, neutral.r, neutral.g, neutral.b, warm.r, warm.g, warm.b, candle.r, candle.g, candle.b);
  CHECK(cool.r >= 250 && cool.g >= 240 && cool.b >= 235);   // close to plain white
  CHECK(warm.r == 255 && near(warm.g, 167, 6) && near(warm.b, 87, 8));
  // Warmer = less blue and less green, all the way down.
  CHECK(cool.b > neutral.b && neutral.b > warm.b && warm.b > candle.b);
  CHECK(cool.g > neutral.g && neutral.g > warm.g && warm.g > candle.g);
  CHECK(miredsToRgb(0) == miredsToRgb(25));   // nonsense values don't crash
}

// ---- From Home -----------------------------------------------------------------------------------

TEST(home_inbox_keeps_the_newest_of_each_kind) {
  HomeInbox inbox;
  CHECK(inbox.take().empty());
  inbox.postLevel(10);
  inbox.postLevel(200);              // a slider dragged in Home
  inbox.postHueSat(40, 200);
  inbox.postTemperature(370);        // then a white preset: the colour is the white
  inbox.postOn(true);
  const HomeChanges c = inbox.take();
  CHECK(c.hasLevel && c.level == 200);
  CHECK(c.colour == HomeChanges::Colour::Temperature && c.mireds == 370);
  CHECK(c.hasOn && c.on);
  CHECK(inbox.take().empty());       // taken
  inbox.postTemperature(250);
  inbox.postHueSat(1, 2);            // and a colour after a white: the colour wins
  CHECK(inbox.take().colour == HomeChanges::Colour::HueSat);
}

TEST(home_on_off_and_brightness_never_change_the_mode) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  settings.setMode(Mode::Gradient, 0);
  HomeView view;
  HomeChanges c;
  c.hasOn = true;
  c.on = false;
  c.hasLevel = true;
  c.level = 127;
  CHECK(applyHomeChanges(c, settings, &view, 0));
  CHECK(settings.current().mode == Mode::Gradient);
  CHECK(!settings.current().on);
  CHECK_EQ(settings.current().brightness, 127);
}

TEST(home_colour_switches_to_solid_from_every_mode) {
  for (Mode m : {Mode::SpectrumSnappy, Mode::SpectrumSmooth, Mode::Solid, Mode::Gradient, Mode::Rainbow,
                 Mode::Twinkle}) {
    FakeSettingsStore store;
    SettingsManager settings(store);
    settings.begin();
    settings.setMode(m, 0);
    const Rgb colourB = settings.current().colourB;
    HomeView view;
    HomeChanges c;
    c.colour = HomeChanges::Colour::HueSat;
    c.hue = 169;   // blue
    c.saturation = 254;
    applyHomeChanges(c, settings, &view, 0);
    CHECK(settings.current().mode == Mode::Solid);
    CHECK(settings.current().colourA == hueSatToRgb(169, 254));
    CHECK(settings.current().colourB == colourB);   // colour B is left alone
  }
}

TEST(home_same_colour_as_colour_a_still_switches_to_solid) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  settings.setMode(Mode::Gradient, 0);
  settings.setColourA(hueSatToRgb(20, 254), 0);   // Home picks the very same colour
  HomeView view;
  HomeChanges c;
  c.colour = HomeChanges::Colour::HueSat;
  c.hue = 20;
  c.saturation = 254;
  CHECK(applyHomeChanges(c, settings, &view, 0));
  CHECK(settings.current().mode == Mode::Solid);
}

TEST(home_white_preset_becomes_a_solid_white) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  settings.setMode(Mode::Rainbow, 0);
  HomeView view;
  HomeChanges c;
  c.colour = HomeChanges::Colour::Temperature;
  c.mireds = 370;
  applyHomeChanges(c, settings, &view, 0);
  CHECK(settings.current().mode == Mode::Solid);
  CHECK(settings.current().colourA == miredsToRgb(370));
}

TEST(home_several_changes_are_one_settings_change) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  const uint32_t before = settings.revision();
  HomeView view;
  HomeChanges c;
  c.hasOn = true;
  c.on = true;
  c.hasLevel = true;
  c.level = 50;
  c.colour = HomeChanges::Colour::HueSat;
  c.hue = 100;
  c.saturation = 100;
  applyHomeChanges(c, settings, &view, 0);
  CHECK_EQ(settings.revision(), before + 1);
  CHECK(!applyHomeChanges(HomeChanges{}, settings, &view, 0));   // nothing: no change
}

// ---- To Home ---------------------------------------------------------------------------------------

TEST(home_follows_web_page_changes) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  HomeView view;
  HomeUpdate u = updateForHome(settings.current(), &view);   // first time: everything
  CHECK(u.sendOn && u.sendLevel && u.sendColour);
  u = updateForHome(settings.current(), &view);              // nothing changed since
  CHECK(!u.sendOn && !u.sendLevel && !u.sendColour);

  settings.setBrightness(200, 0);                             // web page slider
  u = updateForHome(settings.current(), &view);
  CHECK(u.sendLevel && !u.sendOn && !u.sendColour);
  CHECK_EQ(view.level, 200);

  settings.setColourA(Rgb{0, 255, 0}, 0);                     // web page colour picker
  u = updateForHome(settings.current(), &view);
  CHECK(u.sendColour);
  CHECK(near(view.hue, 85, 1) && view.saturation == 254);

  settings.setOn(false, 0);
  CHECK(updateForHome(settings.current(), &view).sendOn);
}

TEST(home_change_from_home_is_not_sent_back) {
  // Home picks a colour and a brightness: nothing should bounce back to Home, so it
  // keeps showing exactly what was chosen (and a white stays a white there).
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  HomeView view;
  updateForHome(settings.current(), &view);
  for (const HomeChanges::Colour kind : {HomeChanges::Colour::HueSat, HomeChanges::Colour::Temperature}) {
    HomeChanges c;
    c.hasLevel = true;
    c.level = 77;
    c.colour = kind;
    c.hue = 33;
    c.saturation = 199;
    c.mireds = 400;
    applyHomeChanges(c, settings, &view, 0);
    const HomeUpdate u = updateForHome(settings.current(), &view);
    CHECK(!u.sendOn && !u.sendLevel && !u.sendColour);
    CHECK(view.colourIsTemperature == (kind == HomeChanges::Colour::Temperature));
  }
}

TEST(home_app_names) {
  CHECK(homeAppName(0x1349, "") == "Apple Home");
  CHECK(homeAppName(0x6006, "whatever") == "Google Home");
  CHECK(homeAppName(0x1234, "Home Assistant") == "Home Assistant");
  CHECK(homeAppName(0x1234, "") == "another smart home app");
}

TEST(home_qr_text_from_the_librarys_link) {
  const std::string base = "https://project-chip.github.io/connectedhomeip/qrcode.html?data=";
  CHECK(qrPayloadFromUrl(base + "MT%3AY.K9042C00KA0648G00") == "MT:Y.K9042C00KA0648G00");
  CHECK(qrPayloadFromUrl(base + "MT:Y.K9042C00KA0648G00") == "MT:Y.K9042C00KA0648G00");   // not encoded
  CHECK(qrPayloadFromUrl(base + "MT%3AABC&x=1") == "MT:ABC");
  CHECK(qrPayloadFromUrl("https://example.com/") == "");
  CHECK(qrPayloadFromUrl("") == "");
}

// ---- Crash-loop safety net ----------------------------------------------------------------

TEST(home_crash_guard_pauses_after_two_crashes_in_a_row) {
  CrashGuard::State ram{0xDEADBEEF, 7, 9};   // junk, as after a power cut
  {
    CrashGuard g(ram);
    g.begin(false);
    CHECK(g.allowed() && g.crashes() == 0);
    g.starting();                       // Matter starts... and the board crashes
  }
  {
    CrashGuard g(ram);
    g.begin(true);
    CHECK(g.allowed() && g.crashes() == 1);   // one crash: try again
    g.starting();
  }
  {
    CrashGuard g(ram);
    g.begin(true);
    CHECK(!g.allowed());                // two in a row: paused
  }
  {
    CrashGuard g(ram);
    g.begin(false);                     // restarted on purpose (update, strip setup): still paused
    CHECK(!g.allowed());
    g.reset();                          // "turn back on" on the web page
  }
  CrashGuard g(ram);
  g.begin(false);
  CHECK(g.allowed());
}

TEST(home_crash_guard_ignores_crashes_once_stable_or_not_on_trial) {
  CrashGuard::State ram{};
  CrashGuard g(ram);
  g.begin(false);
  g.starting();
  g.stable();                           // ran 60 s: later crashes aren't Matter starting up
  CrashGuard a(ram);
  a.begin(true);
  CHECK(a.allowed() && a.crashes() == 0);
  CrashGuard b(ram);                    // a crash before Matter even started doesn't count
  b.begin(true);
  CHECK(b.crashes() == 0);
}

TEST(home_crash_guard_intended_restart_on_trial_clears_count) {
  CrashGuard::State ram{};
  CrashGuard g(ram);
  g.begin(false);
  g.starting();
  CrashGuard a(ram);
  a.begin(true);                        // one crash
  a.starting();
  CrashGuard b(ram);
  b.begin(false);                       // then a deliberate restart before stable
  CHECK(b.crashes() == 0 && b.allowed());
}
