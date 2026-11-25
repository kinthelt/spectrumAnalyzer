// How Apple Home and Google Home (through Matter) and the lights' settings
// affect each other. Plain C++: unit-tested on a PC; MatterBridge connects it
// to the Matter library.
//
// From Home (decided in step 9):
//   - on/off and brightness change only that; the mode never changes for them;
//   - a colour or a white (colour temperature) switches to Solid and becomes colour A.
// To Home: on/off, brightness and colour A follow the lights, whichever way they
// were changed (web page, presets, serial console), so the Home apps match.
#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "../settings/SettingsManager.h"
#include "../util/Rgb.h"

namespace home {

// ---- Conversions ----------------------------------------------------------------------------
// Matter brightness ("level") is 1-254; ours is 0-255 (0 only via "off"). The mapping is
// exact both ways for every Matter level, so a value never drifts as it goes back and forth.
uint8_t levelToBrightness(uint8_t level);
uint8_t brightnessToLevel(uint8_t brightness);

// Matter hue and saturation are 0-254 (hue 254 = all the way round the colour wheel).
// The colour is returned at full strength: brightness is separate.
Rgb hueSatToRgb(uint8_t hue, uint8_t saturation);
void rgbToHueSat(const Rgb& colour, uint8_t* hue, uint8_t* saturation);

// A white of the given colour temperature (mireds = 1,000,000 / kelvin; Home's whites
// are about 153-500, 6500-2000 K), mixed from red, green and blue at full strength.
Rgb miredsToRgb(uint16_t mireds);

// ---- From Home to the lights ---------------------------------------------------------------
// Changes from Home arrive on Matter's own task; the settings belong to the Arduino
// loop. So they are posted here and the loop takes them. Several changes between two
// loops collapse into the newest of each kind (one colour: whichever came last).
struct HomeChanges {
  bool hasOn = false;
  bool on = false;
  bool hasLevel = false;
  uint8_t level = 0;
  enum class Colour : uint8_t { None, HueSat, Temperature } colour = Colour::None;
  uint8_t hue = 0, saturation = 0;
  uint16_t mireds = 0;

  bool empty() const { return !hasOn && !hasLevel && colour == Colour::None; }
};

class HomeInbox {
 public:
  void postOn(bool on);
  void postLevel(uint8_t level);
  void postHueSat(uint8_t hue, uint8_t saturation);
  void postTemperature(uint16_t mireds);
  // Takes everything posted so far (and leaves the inbox empty).
  HomeChanges take();

 private:
  std::mutex mutex_;
  HomeChanges pending_;
};

// What Home's values look like, as last sent to Home or received from it.
struct HomeView {
  bool on = false;
  uint8_t level = 1;
  Rgb colour;                   // colour A this view describes
  bool colourIsTemperature = false;
  uint8_t hue = 0, saturation = 0;
  uint16_t mireds = 0;
};

// Applies changes from Home to the settings (one settings change for all of them).
// Returns true if anything changed. view is updated to what Home now shows.
bool applyHomeChanges(const HomeChanges& changes, SettingsManager& settings, HomeView* view,
                      uint32_t nowMillis);

// ---- From the lights to Home ----------------------------------------------------------------
// Which of Home's values need sending so Home matches the settings.
struct HomeUpdate {
  bool sendOn = false;
  bool sendLevel = false;
  bool sendColour = false;   // as hue and saturation
};

// Compares settings with what Home shows; updates view to match what will be sent.
// A colour that came from Home (including a white) isn't sent back while colour A is
// still that colour, so Home keeps showing exactly what was chosen there.
HomeUpdate updateForHome(const LightSettings& settings, HomeView* view);

// ---- Pairings ---------------------------------------------------------------------------------
// A name for a home app the lights are paired with, for the Settings page. Each app
// leaves its Matter vendor ID when it pairs; some also leave a label.
std::string homeAppName(uint16_t vendorId, const std::string& label);

// The QR code's text ("MT:...") from the Matter library's QR code web link,
// "https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A...".
// "" if the link has no data.
std::string qrPayloadFromUrl(const std::string& url);

}  // namespace home
