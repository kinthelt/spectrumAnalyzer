// Unit tests for src/audio: FilterBank and LevelTracker.

#include <cmath>
#include <random>
#include <vector>

#include "TestFramework.h"
#include "audio/FilterBank.h"
#include "audio/LevelTracker.h"
#include "config/Config.h"

namespace {

constexpr int kBands = FilterBank::kMaxBands;
constexpr int kBlock = FilterBank::kBlock;
constexpr float kFs = config::kSampleRate;
constexpr float kBlockSeconds = static_cast<float>(kBlock) / kFs;

double db(double power) { return 10.0 * std::log10(power + 1e-30); }

FilterBank makeBank() {
  FilterBank bank;
  bank.begin(kBands, kFs, config::kBandLowHz, config::kBandHighHz, config::kBandQScale,
             config::kMinBandwidthHz);
  return bank;
}

int nearestBand(const FilterBank& bank, double hz) {
  int best = 0;
  for (int i = 1; i < kBands; ++i)
    if (std::fabs(std::log(bank.centreHz(i) / hz)) < std::fabs(std::log(bank.centreHz(best) / hz)))
      best = i;
  return best;
}

// Runs a signal through a fresh bank; returns mean band power over the second half.
template <class Gen>
std::vector<double> measure(Gen gen, double seconds) {
  FilterBank bank = makeBank();
  std::vector<double> acc(kBands, 0.0);
  float block[kBlock];
  const int blocks = static_cast<int>(seconds / kBlockSeconds);
  int counted = 0;
  long n = 0;
  for (int b = 0; b < blocks; ++b) {
    for (int s = 0; s < kBlock; ++s) block[s] = gen(n++);
    bank.process(block);
    if (b >= blocks / 2) {
      ++counted;
      for (int i = 0; i < kBands; ++i) acc[i] += bank.power(i);
    }
  }
  for (double& a : acc) a /= counted;
  return acc;
}

auto sine(double hz, double amplitude) {
  return [=](long n) { return static_cast<float>(amplitude * std::sin(2 * M_PI * hz * n / kFs)); };
}

}  // namespace

// ---- FilterBank -------------------------------------------------------------------

TEST(filterbank_configuration) {
  FilterBank bank;
  CHECK(bank.begin(kBands, kFs, config::kBandLowHz, config::kBandHighHz, config::kBandQScale,
                   config::kMinBandwidthHz));
  CHECK(std::fabs(bank.centreHz(0) - config::kBandLowHz) < 0.01f);
  CHECK(std::fabs(bank.centreHz(kBands - 1) - config::kBandHighHz) < 1.0f);
  CHECK_EQ(bank.stageCount(), 8);
  std::printf("      %.2fM band-samples/s\n", bank.bandSamplesPerSecond() / 1e6);
}

TEST(filterbank_every_band_peaks_at_its_own_centre) {
  // A sine at amplitude 0.1 has power 0.005 = -23 dB; each band should read that
  // at its own centre and be the loudest band there.
  const FilterBank ref = makeBank();
  double worstGainError = 0;
  int wrongPeaks = 0;
  for (int i = 0; i < kBands; ++i) {
    const double hz = ref.centreHz(i);
    const auto p = measure(sine(hz, 0.1), std::fmax(0.5, 30.0 / hz));
    int loudest = 0;
    for (int j = 1; j < kBands; ++j)
      if (p[j] > p[loudest]) loudest = j;
    if (loudest != i) ++wrongPeaks;
    worstGainError = std::fmax(worstGainError, std::fabs(db(p[i]) - db(0.005)));
  }
  std::printf("      worst centre gain error %.2f dB\n", worstGainError);
  CHECK_EQ(wrongPeaks, 0);
  CHECK(worstGainError < 1.0);
}

TEST(filterbank_selectivity_and_no_aliasing) {
  const FilterBank ref = makeBank();
  for (double hz : {45.0, 100.0, 440.0, 1000.0, 2500.0, 5000.0, 9000.0, 14000.0}) {
    const auto p = measure(sine(hz, 0.1), 0.6);
    const int centre = nearestBand(ref, hz);
    double worstFar = -1000, worstFive = -1000;
    for (int j = 0; j < kBands; ++j) {
      const double rel = db(p[j]) - db(p[centre]);
      if (std::abs(j - centre) > 17) worstFar = std::fmax(worstFar, rel);    // > 1 octave
      if (std::abs(j - centre) == 5) worstFive = std::fmax(worstFive, rel);
    }
    CHECK(worstFar < -40.0);    // anything over an octave away: skirt or aliasing
    CHECK(worstFive < -15.0);   // 5 LEDs away (bass bands are widened, so this is loose)
  }
}

TEST(filterbank_pink_noise_reads_flat) {
  std::mt19937 rng(1);
  std::normal_distribution<float> gauss(0, 1);
  float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;   // Paul Kellet pink filter
  auto pink = [&](long) {
    const float w = gauss(rng);
    b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
    b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
    b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
    const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
    b6 = w * 0.115926f;
    return out * 0.02f;
  };
  const auto p = measure(pink, 12.0);
  // Only the constant-Q bands: bass bands widened by kMinBandwidthHz collect more noise.
  const FilterBank ref = makeBank();
  double lo = 1e9, hi = -1e9;
  for (int i = 0; i < kBands; ++i) {
    if (ref.q(i) < ref.q(kBands - 1) * 0.999f) continue;
    lo = std::fmin(lo, db(p[i]));
    hi = std::fmax(hi, db(p[i]));
  }
  CHECK(hi - lo < 6.0);
}

// ---- LevelTracker (fed synthetic band powers) ----------------------------------------

namespace {

struct TrackerRig {
  FilterBank bank = makeBank();
  LevelTracker tracker;
  AudioFrame frame;
  std::mt19937 rng{7};
  std::vector<float> noiseDb;   // per-band room noise level
  std::vector<float> jitterDb;  // per-band frame-to-frame noise variation, +/- dB

  TrackerRig() {
    float centres[kBands];
    for (int i = 0; i < kBands; ++i) centres[i] = bank.centreHz(i);
    tracker.begin(kBands, centres);
    // Room noise roughly like the bench: about -105 dB per band.
    for (int i = 0; i < kBands; ++i) noiseDb.push_back(-105.0f);
    jitterDb.assign(kBands, 3.0f);
  }

  // Runs for `seconds`, with extra per-band power from `extra(band, t)` (dB, or
  // below -200 for none). Noise fluctuates +/-3 dB frame to frame.
  template <class Extra>
  void run(double seconds, Extra extra) {
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    const int frames = static_cast<int>(seconds / kBlockSeconds);
    float power[kBands];
    for (int f = 0; f < frames; ++f) {
      const double t = f * kBlockSeconds;
      for (int i = 0; i < kBands; ++i) {
        double p = std::pow(10.0, (noiseDb[i] + jitterDb[i] * unit(rng)) / 10.0);
        const double e = extra(i, t);
        if (e > -200) p += std::pow(10.0, e / 10.0);
        power[i] = static_cast<float>(p);
      }
      tracker.update(power, kBlockSeconds, &frame);
    }
  }

  void runQuiet(double seconds) {
    run(seconds, [](int, double) { return -1000.0; });
  }
};

}  // namespace

TEST(tracker_quiet_room_is_dark_and_silent) {
  TrackerRig rig;
  rig.runQuiet(10.0);
  CHECK(rig.frame.activeBands <= config::kSilenceMaxActiveBands);
  CHECK(rig.frame.silent);
}

TEST(tracker_tone_lights_its_band_only) {
  TrackerRig rig;
  rig.runQuiet(10.0);
  rig.run(2.0, [](int band, double) { return band == 75 ? -60.0 : -1000.0; });
  CHECK(rig.frame.level[75] > 0.9f);
  CHECK(!rig.frame.silent);
  int otherLit = 0;
  for (int i = 0; i < kBands; ++i)
    if (i != 75 && rig.frame.level[i] > 0) ++otherLit;
  CHECK(otherLit <= 2);
}

TEST(tracker_goes_silent_after_sound_stops) {
  TrackerRig rig;
  rig.runQuiet(10.0);
  rig.run(3.0, [](int band, double) { return band % 3 == 0 ? -55.0 : -1000.0; });
  CHECK(!rig.frame.silent);
  rig.runQuiet(config::kSilenceSeconds + 1.0);
  CHECK(rig.frame.silent);
}

TEST(tracker_silence_survives_room_wobble) {
  // Real rooms wobble: on the bench the overall level swung between -72.5 and
  // -66.6 dBFS with nothing lit. Here one rumbly low band dominates the overall
  // level and swings +/-8 dB, which moves the overall level well past the quiet
  // margin, while staying under that band's own (wide) noise gate.
  TrackerRig rig;
  rig.jitterDb.assign(kBands, 1.0f);
  rig.noiseDb[0] = -70.0f;
  rig.jitterDb[0] = 8.0f;
  rig.runQuiet(15.0);
  int silentFrames = 0, litFrames = 0, frames = 0;
  for (int f = 0; f < static_cast<int>(30.0 / kBlockSeconds); ++f) {
    rig.runQuiet(kBlockSeconds);
    ++frames;
    if (rig.frame.silent) ++silentFrames;
    if (rig.frame.activeBands > 0) ++litFrames;
  }
  std::printf("      wobbling room: silent %.1f%% of frames, anything lit %.2f%%\n",
              100.0 * silentFrames / frames, 100.0 * litFrames / frames);
  CHECK(litFrames < frames / 100);
  CHECK_EQ(silentFrames, frames);
}

TEST(tracker_floors_do_not_climb_during_long_music) {
  // On the bench, music at -50 to -60 dBFS had a steady enough overall level
  // that the old design's quiet-level estimate crept up ~0.3 dB/s and would
  // have learned the music as noise. Music here: every band random, at a level
  // similar to the bench, with a beat pulsing the whole spectrum +/-3 dB twice a
  // second (real music is never steady for minutes on end).
  TrackerRig rig;
  rig.runQuiet(10.0);
  double before = 0;
  for (int i = 0; i < kBands; ++i) before += rig.tracker.floorDb(i);
  const float quietBefore = rig.frame.quietRefDb;
  std::mt19937 rng(3);
  std::uniform_real_distribution<float> music(-85.0f, -70.0f);
  rig.run(300.0, [&](int, double t) {
    const double beat = std::fmod(t * 2.0, 1.0) < 0.25 ? 3.0 : -3.0;
    return static_cast<double>(music(rng)) + beat;
  });
  double after = 0;
  for (int i = 0; i < kBands; ++i) after += rig.tracker.floorDb(i);
  const double riseDb = (after - before) / kBands;
  std::printf("      5 min of music: floors moved %.1f dB, quiet level moved %.1f dB\n", riseDb,
              rig.frame.quietRefDb - quietBefore);
  CHECK(riseDb < 3.0);
  CHECK(rig.frame.quietRefDb - quietBefore < 2.0f);
  CHECK(rig.frame.activeBands > kBands / 2);   // music still shows
  CHECK(!rig.frame.silent);
}

TEST(tracker_silent_again_after_music_stops) {
  // Bench report: after the music stopped, SILENT never came back. Real music
  // leaves some bands empty (lowest bass, top treble); there the room's own noise
  // wobbles under the music, and floors must not sink into those dips.
  TrackerRig rig;
  rig.runQuiet(10.0);
  CHECK(rig.frame.silent);
  std::mt19937 rng(9);
  std::uniform_real_distribution<float> music(-85.0f, -70.0f);
  rig.run(120.0, [&](int band, double t) {
    if (band < 15 || band > 125) return -1000.0;   // no music at the extremes
    return static_cast<double>(music(rng)) + (std::fmod(t * 2.0, 1.0) < 0.25 ? 3.0 : -3.0);
  });
  CHECK(!rig.frame.silent);
  rig.runQuiet(5.0);
  std::printf("      5 s after the music stopped: silent=%d, %d bands lit\n",
              rig.frame.silent ? 1 : 0, rig.frame.activeBands);
  CHECK(rig.frame.silent);
  rig.runQuiet(30.0);
  CHECK(rig.frame.silent);
  CHECK_EQ(rig.frame.activeBands, 0);
}

TEST(tracker_recovers_when_music_was_playing_at_power_on) {
  // Switched on at the plug with music already playing: the warm-up hears music,
  // not the room. After the first pause the floors find the room again and the
  // music shows normally.
  TrackerRig rig;
  std::mt19937 rng(5);
  std::uniform_real_distribution<float> music(-85.0f, -70.0f);
  auto song = [&](int, double t) {
    return static_cast<double>(music(rng)) + (std::fmod(t * 2.0, 1.0) < 0.25 ? 3.0 : -3.0);
  };
  rig.run(30.0, song);
  const int litAtStart = rig.frame.activeBands;
  rig.runQuiet(5.0);   // pause between songs
  rig.run(10.0, song);
  std::printf("      music from power-on: %d bands lit; after a pause: %d\n", litAtStart,
              rig.frame.activeBands);
  CHECK(rig.frame.activeBands > kBands / 2);
}

TEST(tracker_learns_a_new_noise_on_request) {
  // A fan starts: every band 12 dB louder. It isn't learned by itself (the
  // tracker can't tell it from steady music), so it shows until asked to learn.
  TrackerRig rig;
  rig.runQuiet(10.0);
  for (float& n : rig.noiseDb) n += 12.0f;
  rig.runQuiet(60.0);
  const int litAfterAMinute = rig.frame.activeBands;
  rig.tracker.learnNoise();
  rig.runQuiet(config::kLearnNoiseSeconds + 2.0);
  std::printf("      fan noise: %d bands lit after 60 s; after learning: %d\n", litAfterAMinute,
              rig.frame.activeBands);
  CHECK(litAfterAMinute > kBands / 2);
  CHECK(!rig.tracker.learningNoise());
  CHECK(rig.frame.activeBands <= config::kSilenceMaxActiveBands);
  CHECK(rig.frame.silent);
}

TEST(tracker_hum_in_a_quiet_room_is_learned) {
  // A steady hum that lights just one band (a transformer buzz) is learned by
  // itself: the quiet-level estimate can creep up while almost nothing is lit.
  TrackerRig rig;
  rig.runQuiet(10.0);
  rig.run(3.0, [](int band, double) { return band == 20 ? -63.0 : -1000.0; });
  CHECK(rig.frame.level[20] > 0.9f);
  rig.run(90.0, [](int band, double) { return band == 20 ? -63.0 : -1000.0; });
  CHECK(rig.frame.level[20] == 0.0f);
  CHECK(rig.frame.silent);
}

namespace {
// A music-like sound in one band: level pulses 12 dB up and down twice a second.
double pulsing(int band, int target, double baseDb, double t) {
  if (band != target) return -1000.0;
  return baseDb + (std::fmod(t * 2.0, 1.0) < 0.5 ? 6.0 : -6.0);
}
}  // namespace

TEST(tracker_gain_follows_loudness) {
  TrackerRig rig;
  rig.runQuiet(10.0);
  rig.run(2.0, [](int band, double t) { return pulsing(band, 50, -40.0, t); });
  const float loudRef = rig.frame.agcRefDb;
  // Much quieter music: the gain recovers at kAgcReleaseDbPerSecond, so after
  // 5 s the reference has dropped about 15 dB...
  rig.run(5.0, [](int band, double t) { return pulsing(band, 50, -70.0, t); });
  const float drop = loudRef - rig.frame.agcRefDb;
  CHECK(drop > 12.0f && drop < 18.0f);
  // ...and once it has caught up, the quiet music fills the range again.
  rig.run(20.0, [](int band, double t) { return pulsing(band, 50, -70.0, t); });
  float peak = 0;
  for (int f = 0; f < 100; ++f) {
    rig.run(kBlockSeconds, [&](int band, double t) { return pulsing(band, 50, -70.0, t + f * kBlockSeconds); });
    peak = std::fmax(peak, rig.frame.level[50]);
  }
  CHECK(peak > 0.9f);
}

// ---- End to end: synthetic microphone -> filter bank -> tracker ------------------------

TEST(end_to_end_tone_in_a_quiet_room) {
  FilterBank bank = makeBank();
  LevelTracker tracker;
  float centres[kBands];
  for (int i = 0; i < kBands; ++i) centres[i] = bank.centreHz(i);
  tracker.begin(kBands, centres);
  AudioFrame frame;

  std::mt19937 rng(11);
  std::normal_distribution<float> gauss(0, 1);
  const float roomNoise = std::pow(10.0f, -75.0f / 20.0f);   // -75 dBFS, as measured
  float block[kBlock];
  long n = 0;
  auto runFor = [&](double seconds, double toneHz, double toneDbfs) {
    const float amp = toneHz > 0 ? std::pow(10.0f, static_cast<float>(toneDbfs) / 20.0f) * 1.41421f : 0;
    const int blocks = static_cast<int>(seconds / kBlockSeconds);
    for (int b = 0; b < blocks; ++b) {
      for (int s = 0; s < kBlock; ++s, ++n)
        block[s] = roomNoise * gauss(rng) +
                   amp * static_cast<float>(std::sin(2 * M_PI * toneHz * n / kFs));
      bank.process(block);
      float power[kBands];
      for (int i = 0; i < kBands; ++i) power[i] = bank.power(i);
      tracker.update(power, kBlockSeconds, &frame);
    }
  };

  runFor(15.0, 0, 0);   // let the floors settle on the room
  // Flicker: over the next 30 s of quiet room, how often is any LED lit?
  int framesWithAnyLit = 0, frames = 0, silentFrames = 0;
  for (int f = 0; f < static_cast<int>(30.0 / kBlockSeconds); ++f) {
    runFor(kBlockSeconds, 0, 0);
    ++frames;
    if (frame.activeBands > 0) ++framesWithAnyLit;
    if (frame.silent) ++silentFrames;
  }
  std::printf("      quiet room: broadband %.1f dBFS; any LED lit in %.2f%% of frames; silent %.1f%%\n",
              frame.broadbandDb, 100.0 * framesWithAnyLit / frames, 100.0 * silentFrames / frames);
  CHECK(framesWithAnyLit < frames / 100);   // any LED at all lit in under 1% of frames
  CHECK(silentFrames == frames);

  runFor(2.0, 1000.0, -45.0);   // a tone 30 dB above the room
  const int band = nearestBand(bank, 1000.0);
  std::printf("      1 kHz tone: LED %d level %.2f; +-3 LEDs %.2f/%.2f; +-8 LEDs %.2f/%.2f; %d lit\n",
              band, frame.level[band], frame.level[band - 3], frame.level[band + 3],
              frame.level[band - 8], frame.level[band + 8], frame.activeBands);
  CHECK(frame.level[band] > 0.9f);
  CHECK(!frame.silent);
  CHECK(frame.level[band - 3] < 0.85f && frame.level[band + 3] < 0.85f);   // clear peak
  CHECK(frame.level[band - 8] < 0.5f && frame.level[band + 8] < 0.5f);     // dim shoulders
  for (int i = 0; i < kBands; ++i)
    if (std::abs(i - band) > 20) CHECK(frame.level[i] == 0.0f);           // nothing far away

  runFor(config::kSilenceSeconds + 1.5, 0, 0);
  CHECK(frame.silent);
}

TEST(end_to_end_silent_again_after_music_stops) {
  // Same bench report, reproduced with the real filter bank: in narrow bands,
  // noise dips deep and often. When floors were allowed to follow dips during
  // sound, they sank ~6 dB in a minute of quiet music, and afterwards the room's
  // noise lit 20-30 bands so silence never returned.
  FilterBank bank = makeBank();
  LevelTracker tracker;
  float centres[kBands];
  for (int i = 0; i < kBands; ++i) centres[i] = bank.centreHz(i);
  tracker.begin(kBands, centres);
  AudioFrame frame;

  std::mt19937 rng(21);
  std::normal_distribution<float> gauss(0, 1);
  std::uniform_real_distribution<float> pick(200.0f, 4000.0f);
  const float roomNoise = std::pow(10.0f, -75.0f / 20.0f);
  float block[kBlock];
  long n = 0;
  double notes[4] = {440, 660, 880, 1320};
  auto runFor = [&](double seconds, bool music) {
    const int blocks = static_cast<int>(seconds / kBlockSeconds);
    for (int b = 0; b < blocks; ++b) {
      if (music && b % 23 == 0)   // new chord every ~0.25 s
        for (double& f : notes) f = pick(rng);
      const double beat = (b % 47) < 12 ? 1.0 : 0.4;   // a pulse twice a second
      for (int s = 0; s < kBlock; ++s, ++n) {
        float x = roomNoise * gauss(rng);
        if (music)   // quiet music, ~-61 dBFS overall, as on the bench
          for (double f : notes) x += static_cast<float>(0.0015 * beat * std::sin(2 * M_PI * f * n / kFs));
        block[s] = x;
      }
      bank.process(block);
      float power[kBands];
      for (int i = 0; i < kBands; ++i) power[i] = bank.power(i);
      tracker.update(power, kBlockSeconds, &frame);
    }
  };

  runFor(15.0, false);
  CHECK(frame.silent);
  runFor(300.0, true);   // five minutes: floors once sank ~6 dB in the first minute
  CHECK(!frame.silent);
  runFor(5.0, false);
  std::printf("      real filters: 5 s after the music stopped: silent=%d, %d bands lit\n",
              frame.silent ? 1 : 0, frame.activeBands);
  CHECK(frame.silent);
  int litFrames = 0, frames = 0;
  for (int f = 0; f < static_cast<int>(20.0 / kBlockSeconds); ++f) {
    runFor(kBlockSeconds, false);
    ++frames;
    if (frame.activeBands > 0) ++litFrames;
  }
  std::printf("      then any LED lit in %.2f%% of frames\n", 100.0 * litFrames / frames);
  CHECK(frame.silent);
  CHECK(litFrames < frames / 100);
}

