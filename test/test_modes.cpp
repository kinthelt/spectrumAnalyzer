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
  m.render(s, nullptr, 0, out, kN);
  for (int i = 0; i < kN; ++i) CHECK(out[i] == s.colourA);
}

TEST(gradient_runs_from_a_to_b) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Gradient);
  m.render(s, nullptr, 0, out, kN);
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

  m.render(s, nullptr, 0, out, kN);
  CHECK(out[0] == (Rgb{255, 0, 0}));   // starts at red
  for (int i = 1; i < kN; ++i) CHECK(out[i] == out[0]);

  m.render(s, nullptr, config::kRainbowCycleMillis / 3, out, kN);
  CHECK(out[0] == (Rgb{0, 255, 0}));   // a third of the way round: green

  m.render(s, nullptr, config::kRainbowCycleMillis, out, kN);
  CHECK(out[0] == (Rgb{255, 0, 0}));   // full cycle: back to red
}

TEST(twinkle_stays_within_colour_and_has_sensible_density) {
  ModeManager m;
  Rgb out[kN];
  const LightSettings s = settingsWith(Mode::Twinkle);

  long litFrames = 0, frames = 0;
  for (uint32_t t = 0; t < 120000; t += 10) {   // two minutes at 100 fps
    m.render(s, nullptr, t, out, kN);
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
  m.render(s, nullptr, 0, prev, kN);
  for (uint32_t t = 10; t < 30000; t += 10) {
    m.render(s, nullptr, t, out, kN);
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
  for (uint32_t t = 0; t < 5000; t += 10) m.render(s, nullptr, t, out, kN);
  s.mode = Mode::Solid;
  m.render(s, nullptr, 5010, out, kN);
  s.mode = Mode::Twinkle;
  m.render(s, nullptr, 5020, out, kN);   // first frame after switching back
  for (int i = 0; i < kN; ++i) CHECK(isBlack(out[i]));
}

TEST(off_renders_black_in_every_mode) {
  for (uint8_t mode = 0; mode < kModeCount; ++mode) {
    ModeManager m;
    Rgb out[kN];
    LightSettings s = settingsWith(static_cast<Mode>(mode));
    s.on = false;
    m.render(s, nullptr, 1234, out, kN);
    for (int i = 0; i < kN; ++i) CHECK(isBlack(out[i]));
  }
}

TEST(every_mode_renders_without_touching_past_the_end) {
  for (uint8_t mode = 0; mode < kModeCount; ++mode) {
    ModeManager m;
    Rgb out[kN + 1];
    out[kN] = Rgb{1, 2, 3};   // sentinel
    const LightSettings s = settingsWith(static_cast<Mode>(mode));
    for (uint32_t t = 0; t < 3000; t += 10) m.render(s, nullptr, t, out, kN);
    CHECK(out[kN] == (Rgb{1, 2, 3}));
  }
}

// ---- Spectrum modes ----------------------------------------------------------------

namespace {

constexpr uint32_t kFrameMs = 10;

// Audio frame with every band at `level` (and not silent).
AudioFrame audioAt(float level) {
  AudioFrame a;
  for (int i = 0; i < kN; ++i) a.level[i] = level;
  a.silent = false;
  return a;
}

// Fixed colour pure red so brightness is just the red channel.
LightSettings spectrumSettings(Mode mode) {
  LightSettings s = settingsWith(mode);
  s.spectrumColour = SpectrumColour::Fixed;
  s.colourA = Rgb{255, 0, 0};
  return s;
}

struct SpectrumRig {
  ModeManager m;
  LightSettings s;
  Rgb out[kN];
  uint32_t t = 0;
  explicit SpectrumRig(Mode mode) : s(spectrumSettings(mode)) {}
  void step(const AudioFrame* a, int frames = 1) {
    for (int f = 0; f < frames; ++f) {
      t += kFrameMs;
      m.render(s, a, t, out, kN);
    }
  }
};

}  // namespace

TEST(spectrum_brightness_follows_gamma) {
  SpectrumRig rig(Mode::SpectrumSnappy);
  const AudioFrame full = audioAt(1.0f), half = audioAt(0.5f);
  rig.step(&full);
  CHECK_EQ(rig.out[10].r, 255);
  rig.step(&half, 50);   // let it fall to and settle at half level
  // 0.5 ^ 2.2 = 0.218 -> about 56 of 255: levels on a dB scale look even.
  CHECK(rig.out[10].r >= 54 && rig.out[10].r <= 58);
}

TEST(spectrum_snappy_rises_instantly_and_falls_fast) {
  SpectrumRig rig(Mode::SpectrumSnappy);
  const AudioFrame quiet = audioAt(0.0f), loud = audioAt(1.0f);
  rig.step(&quiet, 10);
  rig.step(&loud);
  CHECK_EQ(rig.out[50].r, 255);   // full on the very first frame
  rig.step(&quiet, 20);           // 0.2 s later: falling fast (3/s)...
  CHECK(rig.out[50].r > 0 && rig.out[50].r < 60);
  rig.step(&quiet, 20);           // ...and dark by 0.4 s
  CHECK_EQ(rig.out[50].r, 0);
}

TEST(spectrum_snappy_flashes_on_sudden_peaks_only) {
  SpectrumRig rig(Mode::SpectrumSnappy);
  const AudioFrame quiet = audioAt(0.0f), loud = audioAt(1.0f);
  rig.step(&quiet, 10);
  rig.step(&loud);
  // Sudden jump from 0 to 1: red blended toward white.
  CHECK_EQ(rig.out[30].r, 255);
  CHECK(rig.out[30].g > 200 && rig.out[30].b > 200);
  rig.step(&loud, 30);   // 0.3 s later the flash has faded back to pure red
  CHECK(rig.out[30] == (Rgb{255, 0, 0}));

  // A gentle rise is not a peak: no flash.
  SpectrumRig gentle(Mode::SpectrumSnappy);
  gentle.step(&quiet, 10);
  for (int k = 1; k <= 20; ++k) {
    const AudioFrame a = audioAt(k / 20.0f);
    gentle.step(&a);
    CHECK_EQ(gentle.out[30].g, 0);
  }
}

TEST(spectrum_smooth_rises_softly_holds_then_falls_slowly) {
  SpectrumRig rig(Mode::SpectrumSmooth);
  const AudioFrame quiet = audioAt(0.0f), loud = audioAt(1.0f);
  rig.step(&quiet, 10);
  rig.step(&loud);
  CHECK(rig.m.activeMode() == Mode::SpectrumSmooth);
  CHECK(rig.out[50].r < 255);   // not instant
  rig.step(&loud, 20);
  CHECK(rig.out[50].r > 240);   // nearly full after 0.2 s
  rig.step(&quiet, 30);         // 0.3 s into the 0.4 s hold: still at peak
  CHECK(rig.out[50].r > 240);
  rig.step(&quiet, 40);         // 0.7 s: falling, but slowly (0.8/s)
  CHECK(rig.out[50].r > 60);
  rig.step(&quiet, 150);        // 2.2 s: dark
  CHECK_EQ(rig.out[50].r, 0);
  CHECK_EQ(rig.out[50].g, 0);   // Smooth never flashes
}

TEST(spectrum_fades_to_dark_in_silence_and_returns_at_once) {
  SpectrumRig rig(Mode::SpectrumSmooth);
  AudioFrame a = audioAt(0.8f);
  rig.step(&a, 50);
  const uint8_t lit = rig.out[70].r;
  CHECK(lit > 100);

  a.silent = true;   // (levels left non-zero to show the fade itself darkens)
  rig.step(&a, 100);   // 1 s: half faded
  CHECK(rig.out[70].r > 0 && rig.out[70].r < lit);
  rig.step(&a, 110);   // past 2 s: dark
  CHECK_EQ(rig.out[70].r, 0);

  a.silent = false;
  rig.step(&a);
  CHECK(rig.out[70].r == lit);   // sound brings it straight back
}

TEST(spectrum_dark_without_audio) {
  SpectrumRig rig(Mode::SpectrumSnappy);
  rig.step(nullptr, 50);
  for (int i = 0; i < kN; ++i) CHECK(isBlack(rig.out[i]));
}

TEST(spectrum_colour_rainbow_cycle_or_fixed) {
  // Snappy reaches full brightness at once, so colour can be read directly.
  const AudioFrame full = audioAt(1.0f), held = audioAt(1.0f);
  SpectrumRig rig(Mode::SpectrumSnappy);
  rig.s.spectrumColour = SpectrumColour::RainbowCycle;
  rig.step(&full, 30);   // the jump to full flashes white; let the flash fade
  rig.t = config::kRainbowCycleMillis / 3 - kFrameMs;   // next frame lands on green
  rig.step(&held);
  CHECK(rig.out[0] == (Rgb{0, 255, 0}));
  for (int i = 1; i < kN; ++i) CHECK(rig.out[i] == rig.out[0]);   // whole strip one hue

  rig.s.spectrumColour = SpectrumColour::Fixed;
  rig.s.colourA = Rgb{0, 0, 200};
  rig.step(&held);
  CHECK(rig.out[0] == (Rgb{0, 0, 200}));
}

TEST(spectrum_each_led_follows_its_own_band) {
  SpectrumRig rig(Mode::SpectrumSnappy);
  AudioFrame a = audioAt(0.0f);
  a.level[0] = 1.0f;      // bass end
  a.level[149] = 0.5f;    // treble end
  rig.step(&a, 30);
  CHECK_EQ(rig.out[0].r, 255);
  CHECK(rig.out[149].r > 0 && rig.out[149].r < 100);
  CHECK(isBlack(rig.out[75]));
}

