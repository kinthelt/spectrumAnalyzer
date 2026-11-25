#include "LevelTracker.h"

#include <cmath>

namespace {

constexpr float kTinyPower = 1e-20f;   // keeps log10 finite in digital silence

float toDb(float power) { return 10.0f * std::log10(power + kTinyPower); }

// Fraction of the way to move this frame for a smoothing time constant.
float approach(float dt, float tauSeconds) {
  const float a = dt / tauSeconds;
  return a > 1.0f ? 1.0f : a;
}

}  // namespace

float LevelTracker::gateDb(int band) const {
  const float margin = config::kNoiseGateSigmas * std::sqrt(noiseVarDb2_[band]);
  return floorDb_[band] + (margin > config::kNoiseMarginMinDb ? margin : config::kNoiseMarginMinDb);
}

bool LevelTracker::begin(int bands, const float* centreHz) {
  bands_ = 0;
  if (bands < 1 || !tiltDb_.allocate(bands) || !floorDb_.allocate(bands) || !noiseVarDb2_.allocate(bands) ||
      !smoothDb_.allocate(bands) || !bandDb_.allocate(bands) || !level_.allocate(bands)) {
    return false;
  }
  bands_ = bands;
  for (int i = 0; i < bands_; ++i) {
    tiltDb_[i] = config::kTrebleTiltDbPerOctave * std::log2(centreHz[i] / 1000.0f);
    floorDb_[i] = -120.0f;
    // Start by assuming noisy bands (5 dB spread, a ~15 dB gate) so nothing
    // flickers while the real spread is being learned; it settles within seconds.
    noiseVarDb2_[i] = 25.0f;
  }
  broadFastDb_ = broadSlowDb_ = -120;
  broadDeviationDb_ = 0;
  quietRefDb_ = config::kQuietCeilingDb;
  agcRefDb_ = -120.0f;
  silentSeconds_ = 0;
  silent_ = false;   // must be earned after warm-up
  lastActive_ = 0;
  learnSecondsLeft_ = 0;
  warmupSeconds_ = 0;
  return true;
}

void LevelTracker::update(const float* bandPower, float dt, AudioFrame* out) {
  // 0. Warm-up after boot: the filters settle and the floors snap to the room
  //    (assumed quiet; if music is playing, the floors fall to it within seconds).
  if (warmupSeconds_ < config::kWarmupSeconds) {
    warmupSeconds_ += dt;
    float total = 0;
    for (int i = 0; i < bands_; ++i) {
      total += bandPower[i];
      const float db = toDb(bandPower[i]) + tiltDb_[i];
      floorDb_[i] += (db - floorDb_[i]) * approach(dt, 0.1f);
      if (floorDb_[i] > config::kFloorCeilingDb) floorDb_[i] = config::kFloorCeilingDb;
      smoothDb_[i] = floorDb_[i];
      level_[i] = 0.0f;
    }
    const float broadbandDb = toDb(total);
    quietRefDb_ = broadbandDb < config::kQuietCeilingDb ? broadbandDb : config::kQuietCeilingDb;
    broadFastDb_ = broadSlowDb_ = broadbandDb;
    out->broadbandDb = broadbandDb;
    out->agcRefDb = agcRefDb_;
    out->quietRefDb = quietRefDb_;
    out->activeBands = 0;
    out->silent = true;
    return;
  }

  // 1. Overall level, and whether this is a quiet moment.
  float total = 0;
  for (int i = 0; i < bands_; ++i) total += bandPower[i];
  const float broadbandDb = toDb(total);

  // How steady is the overall level? Compare a fast and a slow average.
  broadFastDb_ += (broadbandDb - broadFastDb_) * approach(dt, 0.1f);
  broadSlowDb_ += (broadbandDb - broadSlowDb_) * approach(dt, 1.0f);
  broadDeviationDb_ +=
      (std::fabs(broadFastDb_ - broadSlowDb_) - broadDeviationDb_) * approach(dt, 1.0f);
  const bool steady = broadDeviationDb_ < config::kSteadyDb;

  // The quiet-level estimate follows the overall level down quickly, and creeps
  // up only while steady with almost nothing lit (last frame), so music can't
  // drag it up.
  if (broadbandDb < quietRefDb_) {
    quietRefDb_ += (broadbandDb - quietRefDb_) * approach(dt, 0.2f);
  } else if (steady && lastActive_ <= config::kSilenceMaxActiveBands) {
    quietRefDb_ += config::kQuietRiseDbPerSecond * dt;
  }
  if (quietRefDb_ > config::kQuietCeilingDb) quietRefDb_ = config::kQuietCeilingDb;
  const bool quietMoment = broadbandDb < quietRefDb_ + config::kQuietMarginDb;

  // Learn floors while silent (decided last frame), or on request.
  const bool learnFloors = silent_ || learnSecondsLeft_ > 0;
  if (learnSecondsLeft_ > 0) learnSecondsLeft_ -= dt;

  // 2. Per-band level in dB with treble tilt; noise floors; loudest band.
  float loudestDb = -1000.0f;
  for (int i = 0; i < bands_; ++i) {
    const float db = toDb(bandPower[i]) + tiltDb_[i];
    bandDb_[i] = db;
    if (db > loudestDb) loudestDb = db;

    // Silence (or a learn request): settle on the average room level, both ways.
    float& floor = floorDb_[i];
    // The band's level averaged over about a second, for spotting a floor that
    // is clearly too high (single frames of noise dip too deep to trust).
    smoothDb_[i] += (db - smoothDb_[i]) * approach(dt, 1.0f);

    if (learnFloors && smoothDb_[i] < floor - config::kNoiseMarginMinDb) {
      // Floor clearly too high (e.g. music was playing at power-on): drop to the
      // room level quickly, and don't count the drop as noise spread.
      floor += (smoothDb_[i] - floor) * approach(dt, 0.3f);
    } else if (learnFloors) {
      float deviation = db - floor;
      floor += deviation * approach(dt, config::kFloorAdaptSeconds);
      // Spread of the noise around the floor. Clipping each frame's deviation
      // stops a sudden change (a door closing) from inflating it.
      if (deviation > 12.0f) deviation = 12.0f;
      if (deviation < -12.0f) deviation = -12.0f;
      noiseVarDb2_[i] +=
          (deviation * deviation - noiseVarDb2_[i]) * approach(dt, config::kNoiseSpreadSeconds);
    }
    // During sound, floors stay frozen. (Letting them follow dips down was
    // tried: noise in narrow bands dips deep, so floors sank ~6 dB a minute
    // under quiet music and the room's noise lit up when the music stopped.)
    if (floor > config::kFloorCeilingDb) floor = config::kFloorCeilingDb;
  }

  // 3. Automatic gain: the top of the window follows the loudest band, quickly
  //    up and slowly down (it can't sink below the loudest band).
  if (loudestDb > agcRefDb_) {
    agcRefDb_ += (loudestDb - agcRefDb_) * approach(dt, config::kAgcAttackSeconds);
  } else {
    agcRefDb_ -= config::kAgcReleaseDbPerSecond * dt;
    if (agcRefDb_ < loudestDb) agcRefDb_ = loudestDb;
  }

  // 4. Level per band, 0..1 across its window. The window runs from the band's
  //    noise gate (or kDisplayRangeDb below the top, if higher) to the top, and
  //    is never narrower than kMinSpanDb, so noise alone can't fill it.
  uint16_t active = 0;
  for (int i = 0; i < bands_; ++i) {
    const float gate = gateDb(i);
    const float windowBottom = agcRefDb_ - config::kDisplayRangeDb;
    const float lo = gate > windowBottom ? gate : windowBottom;
    const float hi = agcRefDb_ > lo + config::kMinSpanDb ? agcRefDb_ : lo + config::kMinSpanDb;
    float level = (bandDb_[i] - lo) / (hi - lo);
    if (level <= 0.0f) {
      level = 0.0f;
    } else {
      ++active;
      if (level > 1.0f) level = 1.0f;
    }
    level_[i] = level;
  }

  // 5. Silence. Entering needs a sustained run of quiet frames; once silent, small
  //    wobbles in the room's level don't end it, only real sound does.
  if (silent_) {
    const bool loud = broadbandDb > quietRefDb_ + config::kQuietMarginDb + config::kSilenceExitDb;
    if (active > config::kSilenceMaxActiveBands || loud) {
      silent_ = false;
      silentSeconds_ = 0;
    }
  } else {
    // Nothing lit counts as silence only if the room is plausibly quiet (so music
    // that the floors wrongly learned at power-on doesn't count); a band or two
    // lit counts during a quiet moment.
    const bool plausiblyQuiet = broadbandDb < config::kQuietCeilingDb + config::kQuietMarginDb;
    const bool quietFrame = (active == 0 && plausiblyQuiet) ||
                            (quietMoment && active <= config::kSilenceMaxActiveBands);
    silentSeconds_ = quietFrame ? silentSeconds_ + dt : 0.0f;
    if (silentSeconds_ >= config::kSilenceSeconds) silent_ = true;
  }

  lastActive_ = active;

  out->broadbandDb = broadbandDb;
  out->agcRefDb = agcRefDb_;
  out->quietRefDb = quietRefDb_;
  out->activeBands = active;
  out->silent = silent_;
  out->learningNoise = learnSecondsLeft_ > 0;
}
