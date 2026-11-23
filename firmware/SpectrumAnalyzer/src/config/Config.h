// Hardware and tuning constants. Everything board- or install-specific lives here.
#pragma once

#include <cstdint>

namespace config {

// ---- Firmware ----------------------------------------------------------------
constexpr const char* kFirmwareVersion = "0.1.0-skeleton";
constexpr uint32_t kSerialBaud = 115200;

// ---- LED strip: Adafruit 1376, 5 m x 30 LED/m --------------------------------
constexpr int kLedDataPin = 7;    // -> SN74AHCT126N pin 2 (1A); pin 3 (1Y) -> 330R -> strip DIN
constexpr int kNumLeds = 150;

// ---- Microphone: ICS-43434 over I2S ------------------------------------------
constexpr int kMicBclkPin = 4;
constexpr int kMicWsPin = 5;
constexpr int kMicDataPin = 6;

// ---- Audio analysis ----------------------------------------------------------
constexpr int kSampleRate = 48000;
// Samples per analysis block: 512 @ 48 kHz = 10.7 ms, ~94 updates per second.
// Must be divisible by 2^(decimation stages - 1); 512 allows up to 9 stages.
constexpr int kAudioBlockSize = 512;
constexpr float kBandLowHz = 40.0f;      // centre of LED 0
constexpr float kBandHighHz = 15000.0f;  // centre of the last LED (mic is rated to ~15 kHz)
// 1.0 = neighbouring bands meet at their -3 dB points. <1 widens them, >1 narrows them.
constexpr float kBandQScale = 1.0f;
// Bands are never narrower than this. A band B Hz wide takes ~1/B s to respond,
// so this keeps bass LEDs from feeling sluggish (40 Hz LED: ~0.14 s at 5 Hz).
constexpr float kMinBandwidthHz = 5.0f;

// ---- Level tracking (band power -> 0..1 level per LED) -------------------------
constexpr float kTrebleTiltDbPerOctave = 3.0f;  // treble boost, pivoting at 1 kHz
constexpr float kDisplayRangeDb = 45.0f;        // dB from dark to full, below the loudest band
constexpr float kAgcAttackSeconds = 0.05f;      // gain drops this fast when music gets louder
constexpr float kAgcReleaseDbPerSecond = 3.0f;  // and recovers this slowly when it gets quieter
constexpr float kMinSpanDb = 20.0f;             // gain never zooms in closer than this above the noise
// A band lights only above its noise gate: its average noise level plus
// kNoiseGateSigmas times how much that noise varies, and at least kNoiseMarginMinDb.
// Narrow bass bands vary a lot (gate ~15 dB above average), treble very little.
constexpr float kNoiseGateSigmas = 3.5f;
constexpr float kNoiseMarginMinDb = 6.0f;
constexpr float kNoiseSpreadSeconds = 1.0f;     // how quickly each band's noise spread is learned
constexpr float kWarmupSeconds = 0.5f;          // after boot: floors snap to the room, LEDs stay dark
constexpr float kFloorAdaptSeconds = 3.0f;      // in quiet moments, floors settle on the room level
// Noise floors are learned automatically only during silence, so music is never
// mistaken for noise. A new noise loud enough to light bands (a fan starting) is
// learned on request: "learn" in Serial Monitor (later a web-page button) learns
// the room for this long. Keep the room free of music while it runs.
constexpr float kLearnNoiseSeconds = 10.0f;
constexpr float kSteadyDb = 1.5f;               // "steady": overall level wobbling less than this
// No band's noise floor is ever above this (tilted dB). Keeps music from being
// learned as noise if it is already playing at power-on. Bench room: ~-105 to -90.
constexpr float kFloorCeilingDb = -78.0f;
// "Quiet moment" detection on the overall level, used for silence:
constexpr float kQuietMarginDb = 6.0f;          // within this of the quietest recent level
constexpr float kQuietRiseDbPerSecond = 0.5f;   // estimate creeps up this fast, only while
                                                // steady and with almost nothing lit
constexpr float kQuietCeilingDb = -64.0f;       // and never above this overall level (dBFS)
// Silence starts after kSilenceSeconds of frames with nothing lit, or at most
// kSilenceMaxActiveBands lit during a quiet moment. It ends when more bands light,
// or the overall level rises kSilenceExitDb above the quiet margin.
constexpr int kSilenceMaxActiveBands = 2;
constexpr float kSilenceSeconds = 1.5f;
constexpr float kSilenceExitDb = 10.0f;

// ---- Audio task --------------------------------------------------------------
constexpr int kAudioTaskCore = 0;        // LEDs and the Arduino loop run on core 1
constexpr int kAudioTaskPriority = 5;
constexpr uint32_t kAudioTaskStackBytes = 8192;

// ---- Power -------------------------------------------------------------------
// FastLED scales brightness down whenever a frame would draw more than this.
// Bench (thin jumper wires): 2000 mA. After install with 14 AWG feeds at both
// ends: raise to 5000 mA (the Mean Well GST60A05 supplies 6 A).
constexpr uint8_t kSupplyVolts = 5;
constexpr uint32_t kPowerLimitMilliamps = 2000;

// ---- Output ------------------------------------------------------------------
constexpr uint8_t kDefaultBrightness = 64;   // 0-255, global brightness
constexpr uint32_t kFrameIntervalMicros = 10000;   // 100 frames per second target
constexpr uint32_t kStatsIntervalMillis = 5000;    // how often stats go to serial

// ---- Lighting modes ----------------------------------------------------------
// Rainbow (and the spectrum modes' rainbow cycle): one full trip round the colour wheel.
constexpr uint32_t kRainbowCycleMillis = 60000;

// Twinkle: each dark LED lights up at random, on average this often per second...
constexpr float kTwinkleStartsPerLedPerSecond = 0.08f;
// ...fading in and out over these times. Average lit share ~ rate x (in + out) ~ 13%.
constexpr float kTwinkleFadeInSeconds = 0.4f;
constexpr float kTwinkleFadeOutSeconds = 1.2f;

}  // namespace config
