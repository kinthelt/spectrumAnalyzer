// Unit tests for src/util/Rgb and src/modes.

#include <algorithm>
#include <cstdlib>

#include "TestFramework.h"
#include "config/Config.h"
#include "modes/ModeManager.h"
#include "util/Rgb.h"

namespace {

constexpr int kN = config::kNumLeds;

LightSettings settingsWith(Mode mode) {
  LightSettings s;
  s.mode = mode;
  s.colourA = Rgb{255, 100, 0};
  s.colourB = Rgb{0, 50, 200};
  s.on = true;
  return s;
}

bool isBlack(const Rgb& c) { return c.r == 0 && c.g == 0 && c.b == 0; }

}  // namespace

// ---- Colour maths -----------------------------------------------------------------

TEST(hue_primaries) {
  CHECK(hueToRgb(0.0f) == (Rgb{255, 0, 0}));
  CHECK(hueToRgb(1.0f / 3) == (Rgb{0, 255, 0}));
  CHECK(hueToRgb(2.0f / 3) == (Rgb{0, 0, 255}));
  CHECK(hueToRgb(1.0f) == hueToRgb(0.0f));    // wraps
  CHECK(hueToRgb(-0.5f) == hueToRgb(0.5f));   // negative wraps too
}

TEST(hue_is_always_fully_saturated) {
  for (int i = 0; i < 1000; ++i) {
    const Rgb c = hueToRgb(i / 1000.0f);
    const int maxC = std::max({c.r, c.g, c.b});
    const int minC = std::min({c.r, c.g, c.b});
    CHECK_EQ(maxC, 255);
    CHECK_EQ(minC, 0);
  }
}

TEST(lerp_and_scale) {
  const Rgb a{0, 100, 200}, b{200, 100, 0};
  CHECK(lerp(a, b, 0.0f) == a);
  CHECK(lerp(a, b, 1.0f) == b);
  CHECK(lerp(a, b, 0.5f) == (Rgb{100, 100, 100}));
  CHECK(lerp(a, b, 2.0f) == b);   // clamped
  CHECK(scale(Rgb{200, 100, 50}, 0.5f) == (Rgb{100, 50, 25}));
  CHECK(scale(Rgb{200, 100, 50}, 0.0f) == Rgb{});
}

// ---- Modes ------------------------------------------------------------------------------

TEST(solid_fills_colour_a) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Solid);
  m.render(s, 0, out, kN);
  for (int i = 0; i < kN; ++i) CHECK(out[i] == s.colourA);
}

TEST(gradient_runs_from_a_to_b) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Gradient);
  m.render(s, 0, out, kN);
  CHECK(out[0] == s.colourA);
  CHECK(out[kN - 1] == s.colourB);
  for (int i = 1; i < kN; ++i) {   // red falls and blue rises steadily
    CHECK(out[i].r <= out[i - 1].r);
    CHECK(out[i].b >= out[i - 1].b);
  }
}

TEST(rainbow_whole_strip_one_colour_cycling) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Rainbow);

  m.render(s, 0, out, kN);
  CHECK(out[0] == (Rgb{255, 0, 0}));   // starts at red
  for (int i = 1; i < kN; ++i) CHECK(out[i] == out[0]);

  m.render(s, config::kRainbowCycleMillis / 3, out, kN);
  CHECK(out[0] == (Rgb{0, 255, 0}));   // a third of the way round: green

  m.render(s, config::kRainbowCycleMillis, out, kN);
  CHECK(out[0] == (Rgb{255, 0, 0}));   // full cycle: back to red
}

TEST(twinkle_stays_within_colour_and_has_sensible_density) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Twinkle);

  long litFrames = 0, frames = 0;
  for (uint32_t t = 0; t < 120000; t += 10) {   // two minutes at 100 fps
    m.render(s, t, out, kN);
    for (int i = 0; i < kN; ++i) {
      CHECK(out[i].r <= s.colourA.r);
      CHECK(out[i].g <= s.colourA.g);
      CHECK(out[i].b <= s.colourA.b);
      if (!isBlack(out[i])) ++litFrames;
    }
    ++frames;
  }
  // Expected lit share ~ rate x (fade in + fade out) = 0.08 x 1.6 ~ 13%.
  const double litShare = static_cast<double>(litFrames) / (frames * kN);
  CHECK(litShare > 0.08);
  CHECK(litShare < 0.18);
}

TEST(twinkle_fades_smoothly) {
  ModeManager m;
  Rgb prev[kN], out[kN];
  const LightSettings s = settingsWith(Mode::Twinkle);
  m.render(s, 0, prev, kN);
  for (uint32_t t = 10; t < 30000; t += 10) {
    m.render(s, t, out, kN);
    for (int i = 0; i < kN; ++i) {
      // Red channel of colour A is 255; per 10 ms frame it may move at most a
      // small step (fade-in of 0.4 s on a squared curve is the fastest change).
      CHECK(std::abs(out[i].r - prev[i].r) <= 13);
      prev[i] = out[i];
    }
  }
}

TEST(twinkle_restarts_dark_when_reselected) {
  ModeManager m;
  Rgb out[kN];
  LightSettings s = settingsWith(Mode::Twinkle);
  for (uint32_t t = 0; t < 5000; t += 10) m.render(s, t, out, kN);
  s.mode = Mode::Solid;
  m.render(s, 5010, out, kN);
  s.mode = Mode::Twinkle;
  m.render(s, 5020, out, kN);   // first frame after switching back
  for (int i = 0; i < kN; ++i) CHECK(isBlack(out[i]));
}

TEST(off_renders_black_in_every_mode) {
  for (uint8_t mode = 0; mode < kModeCount; ++mode) {
    ModeManager m;
    Rgb out[kN];
    LightSettings s = settingsWith(static_cast<Mode>(mode));
    s.on = false;
    m.render(s, 1234, out, kN);
    for (int i = 0; i < kN; ++i) CHECK(isBlack(out[i]));
  }
}

TEST(spectrum_placeholder_uses_spectrum_colouring) {
  ModeManager m;
  Rgb out[kN];
  LightSettings s = settingsWith(Mode::SpectrumSmooth);

  s.spectrumColour = SpectrumColour::Fixed;
  m.render(s, 0, out, kN);
  CHECK(out[0] == s.colourA);   // comet head starts at LED 0

  s.spectrumColour = SpectrumColour::RainbowCycle;
  m.render(s, 0, out, kN);
  CHECK(out[0] == (Rgb{255, 0, 0}));   // rainbow cycle starts at red
}

TEST(every_mode_renders_without_touching_past_the_end) {
  for (uint8_t mode = 0; mode < kModeCount; ++mode) {
    ModeManager m;
    Rgb out[kN + 1];
    out[kN] = Rgb{1, 2, 3};   // sentinel
    const LightSettings s = settingsWith(static_cast<Mode>(mode));
    for (uint32_t t = 0; t < 3000; t += 10) m.render(s, t, out, kN);
    CHECK(out[kN] == (Rgb{1, 2, 3}));
  }
}
