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

// ---- Microphone: ICS-43434 over I2S (used from build step 4) -----------------
constexpr int kMicBclkPin = 4;
constexpr int kMicWsPin = 5;
constexpr int kMicDataPin = 6;

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
