// Hardware and tuning constants. Everything board- or install-specific lives here.
#pragma once

#include <cstdint>

namespace config {

// ---- Firmware ----------------------------------------------------------------
constexpr const char* kFirmwareVersion = "0.8.0";
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

// ---- Spectrum modes ----------------------------------------------------------
// Levels are 0..1 on a dB scale; brightness = level ^ gamma so steps look even.
constexpr float kSpectrumGamma = 2.2f;
// Snappy: instant rise, fast fall, and a white flash on a sudden new peak.
constexpr float kSnappyFallPerSecond = 3.0f;    // full to dark in ~0.33 s
constexpr float kFlashJump = 0.25f;             // a jump this big in one frame...
constexpr float kFlashMinLevel = 0.5f;          // ...to at least this level flashes
constexpr float kFlashSeconds = 0.25f;          // flash fades back to colour over this
// Smooth: softened rise, then each LED holds its peak before falling slowly.
constexpr float kSmoothAttackSeconds = 0.06f;
constexpr float kSmoothHoldSeconds = 0.4f;
constexpr float kSmoothFallPerSecond = 0.8f;    // full to dark in ~1.25 s
// Silence: spectrum modes fade to dark over this; sound brings them back at once.
constexpr float kSilenceFadeSeconds = 2.0f;
// No audio frame for this long (audio failed) counts as silence.
constexpr uint32_t kAudioStaleMillis = 500;

// ---- Network ------------------------------------------------------------------
constexpr const char* kHostname = "spectrum";              // http://spectrum.local
constexpr const char* kSetupNetworkName = "Spectrum-Setup";
// Saved Wi-Fi not reachable for this long after boot: open the setup network.
constexpr uint32_t kWifiConnectTimeoutMillis = 2u * 60u * 1000u;
// Connected Wi-Fi lost for this long (router off, password changed): open setup.
constexpr uint32_t kWifiLostBeforeSetupMillis = 5u * 60u * 1000u;
// Setup network closes after this long and the saved Wi-Fi is tried again.
constexpr uint32_t kSetupPortalMillis = 10u * 60u * 1000u;
// Switching off at the plug this many times, each time within kPowerCycleWindow
// of switching on, opens the setup network (the board is above the ceiling).
constexpr int kPowerCyclesForSetup = 3;
constexpr uint32_t kPowerCycleWindowMillis = 5000;

// ---- Firmware updates over Wi-Fi ----------------------------------------------
// After an update, the new firmware counts as good once it has been on the home
// Wi-Fi with the web page running for kUpdateGoodAfterMillis in a row. If that
// hasn't happened kUpdateGiveUpMillis after starting (or it crashes or restarts
// first), the board goes back to the previous firmware.
constexpr uint32_t kUpdateGoodAfterMillis = 30u * 1000;
constexpr uint32_t kUpdateGiveUpMillis = 5u * 60 * 1000;
constexpr uint32_t kUpdateStallMillis = 15u * 1000;       // upload gives up if no data this long
constexpr uint32_t kUpdateRestartDelayMillis = 1500;      // green flash, and the reply reaches the phone
constexpr uint32_t kUpdateFailFlashMillis = 2000;         // red flash, then back to normal
constexpr uint8_t kUpdateBrightness = 80;                 // update colours show even if the lights were off
// Bench test of the rollback only: set true, upload this build over Wi-Fi, and it
// never declares itself good, so after kUpdateGiveUpMillis the previous firmware
// returns. Always false otherwise.
constexpr bool kTestUpdateNeverConfirms = false;

// ---- LED task -----------------------------------------------------------------
// Core 1, above the Arduino loop (priority 1), so network work never delays frames.
constexpr int kLedTaskCore = 1;
constexpr int kLedTaskPriority = 3;
constexpr uint32_t kLedTaskStackBytes = 8192;

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
