#include "HomeRules.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace home {

// ---- Conversions ----------------------------------------------------------------------------

uint8_t levelToBrightness(uint8_t level) {
  if (level < 1) level = 1;
  if (level >= 254) return 255;   // Home's full is our full
  return level;
}

uint8_t brightnessToLevel(uint8_t brightness) {
  if (brightness < 1) return 1;
  if (brightness >= 254) return 254;
  return brightness;
}

Rgb hueSatToRgb(uint8_t hue, uint8_t saturation) {
  const float h = std::min<float>(hue, 254) / 254.0f * 6.0f;   // 0..6 sectors
  const float s = std::min<float>(saturation, 254) / 254.0f;
  const int sector = static_cast<int>(h) % 6;
  const float f = h - std::floor(h);
  const float p = 1.0f - s, q = 1.0f - s * f, t = 1.0f - s * (1.0f - f);
  float r = 0, g = 0, b = 0;
  switch (sector) {
    case 0: r = 1, g = t, b = p; break;
    case 1: r = q, g = 1, b = p; break;
    case 2: r = p, g = 1, b = t; break;
    case 3: r = p, g = q, b = 1; break;
    case 4: r = t, g = p, b = 1; break;
    default: r = 1, g = p, b = q; break;
  }
  auto to8 = [](float x) { return static_cast<uint8_t>(std::lround(x * 255.0f)); };
  return Rgb{to8(r), to8(g), to8(b)};
}

void rgbToHueSat(const Rgb& c, uint8_t* hue, uint8_t* saturation) {
  const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
  const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
  const float d = mx - mn;
  float h = 0;   // in sectors, 0..6
  if (d > 0) {
    if (mx == r) {
      h = std::fmod((g - b) / d + 6.0f, 6.0f);
    } else if (mx == g) {
      h = (b - r) / d + 2.0f;
    } else {
      h = (r - g) / d + 4.0f;
    }
  }
  const float s = mx > 0 ? d / mx : 0;
  long hv = std::lround(h / 6.0f * 254.0f);
  if (hv >= 254) hv = 0;   // all the way round is red again
  *hue = static_cast<uint8_t>(hv);
  *saturation = static_cast<uint8_t>(std::lround(s * 254.0f));
}

Rgb miredsToRgb(uint16_t mireds) {
  // Tanner Helland's fit of blackbody colours, for 1000-40000 K.
  if (mireds < 25) mireds = 25;
  float k = 1000000.0f / mireds / 100.0f;   // kelvin / 100
  k = std::min(std::max(k, 10.0f), 400.0f);
  float r, g, b;
  if (k <= 66) {
    r = 255;
    g = 99.4708025861f * std::log(k) - 161.1195681661f;
    b = k <= 19 ? 0 : 138.5177312231f * std::log(k - 10) - 305.0447927307f;
  } else {
    r = 329.698727446f * std::pow(k - 60, -0.1332047592f);
    g = 288.1221695283f * std::pow(k - 60, -0.0755148492f);
    b = 255;
  }
  auto clamp8 = [](float x) { return static_cast<uint8_t>(std::lround(std::min(std::max(x, 0.0f), 255.0f))); };
  return Rgb{clamp8(r), clamp8(g), clamp8(b)};
}

// ---- From Home to the lights ---------------------------------------------------------------

void HomeInbox::postOn(bool on) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_.hasOn = true;
  pending_.on = on;
}

void HomeInbox::postLevel(uint8_t level) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_.hasLevel = true;
  pending_.level = level;
}

void HomeInbox::postHueSat(uint8_t hue, uint8_t saturation) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_.colour = HomeChanges::Colour::HueSat;
  pending_.hue = hue;
  pending_.saturation = saturation;
}

void HomeInbox::postTemperature(uint16_t mireds) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_.colour = HomeChanges::Colour::Temperature;
  pending_.mireds = mireds;
}

HomeChanges HomeInbox::take() {
  std::lock_guard<std::mutex> lock(mutex_);
  HomeChanges out = pending_;
  pending_ = HomeChanges{};
  return out;
}

bool applyHomeChanges(const HomeChanges& changes, SettingsManager& settings, HomeView* view,
                      uint32_t nowMillis) {
  if (changes.empty()) return false;
  LightSettings next = settings.current();
  if (changes.hasOn) {
    next.on = changes.on;
    view->on = changes.on;
  }
  if (changes.hasLevel) {
    next.brightness = levelToBrightness(changes.level);
    view->level = brightnessToLevel(next.brightness);
  }
  if (changes.colour != HomeChanges::Colour::None) {
    const bool white = changes.colour == HomeChanges::Colour::Temperature;
    next.colourA = white ? miredsToRgb(changes.mireds) : hueSatToRgb(changes.hue, changes.saturation);
    next.mode = Mode::Solid;   // a colour from Home always means Solid
    view->colour = next.colourA;
    view->colourIsTemperature = white;
    view->hue = changes.hue;
    view->saturation = changes.saturation;
    view->mireds = changes.mireds;
  }
  if (next == settings.current()) return false;
  settings.update(next, nowMillis);
  return true;
}

// ---- From the lights to Home ----------------------------------------------------------------

HomeUpdate updateForHome(const LightSettings& s, HomeView* view) {
  HomeUpdate u;
  if (s.on != view->on) {
    view->on = s.on;
    u.sendOn = true;
  }
  const uint8_t level = brightnessToLevel(s.brightness);
  if (level != view->level) {
    view->level = level;
    u.sendLevel = true;
  }
  if (s.colourA != view->colour) {
    view->colour = s.colourA;
    view->colourIsTemperature = false;
    rgbToHueSat(s.colourA, &view->hue, &view->saturation);
    u.sendColour = true;
  }
  return u;
}

// ---- Pairings ---------------------------------------------------------------------------------

std::string homeAppName(uint16_t vendorId, const std::string& label) {
  switch (vendorId) {
    case 0x1349: return "Apple Home";    // Apple's Matter vendor ID
    case 0x6006: return "Google Home";   // Google's
    default: break;
  }
  return label.empty() ? "another smart home app" : label;
}

std::string qrPayloadFromUrl(const std::string& url) {
  const size_t at = url.find("data=");
  if (at == std::string::npos) return "";
  std::string out;
  for (size_t i = at + 5; i < url.size() && url[i] != '&'; ++i) {
    if (url[i] == '%' && i + 2 < url.size()) {
      const char hex[3] = {url[i + 1], url[i + 2], '\0'};
      out += static_cast<char>(std::strtol(hex, nullptr, 16));   // never throws
      i += 2;
    } else {
      out += url[i];
    }
  }
  return out;
}

}  // namespace home
