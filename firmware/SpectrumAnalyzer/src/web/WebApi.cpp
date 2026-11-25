#include "WebApi.h"

#include <cstring>

#include "../util/JsonWriter.h"

namespace web_api {
namespace {

std::string hex(const Rgb& c) {
  char buf[8];
  buf[0] = '#';
  formatHexColour(c, buf + 1);
  return buf;
}

// Strict: digits only, within [lo, hi].
bool parseInt(const char* s, int lo, int hi, int* out) {
  if (s == nullptr || *s == '\0') return false;
  long v = 0;
  for (const char* p = s; *p != 0; ++p) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (*p - '0');
    if (v > hi) return false;
  }
  if (v < lo) return false;
  *out = static_cast<int>(v);
  return true;
}

bool parseBool(const char* s, bool* out) {
  if (std::strcmp(s, "1") == 0 || std::strcmp(s, "true") == 0) {
    *out = true;
    return true;
  }
  if (std::strcmp(s, "0") == 0 || std::strcmp(s, "false") == 0) {
    *out = false;
    return true;
  }
  return false;
}

bool parseSlot(const FormArgs& args, int* slot) {
  int n;
  if (!parseInt(args.get("slot"), 1, SettingsManager::kPresetCount, &n)) return false;
  *slot = n - 1;
  return true;
}

}  // namespace

std::string applyStrip(const FormArgs& args, StripSetupManager& strip, bool* needsRestart) {
  StripSetup next = strip.chosen();
  int v = 0;
  if (const char* s = args.get("ledCount")) {
    if (!parseInt(s, config::kMinLedCount, config::kMaxLedCount, &v)) {
      return "The number of LEDs must be " + std::to_string(config::kMinLedCount) + " to " +
             std::to_string(config::kMaxLedCount) + ".";
    }
    next.ledCount = static_cast<uint16_t>(v);
  }
  if (const char* s = args.get("bassEnd")) {
    if (std::strcmp(s, "left") == 0) {
      next.bassOnRight = false;
    } else if (std::strcmp(s, "right") == 0) {
      next.bassOnRight = true;
    } else {
      return "'bassEnd' must be left or right";
    }
  }
  for (const char* field : {"lowHz", "highHz"}) {
    if (const char* s = args.get(field)) {
      if (!parseInt(s, 0, 99999, &v)) return "Frequencies are whole numbers of Hz.";
      (std::strcmp(field, "lowHz") == 0 ? next.lowHz : next.highHz) = static_cast<float>(v);
    }
  }
  *needsRestart = false;
  return strip.change(next, needsRestart);   // validates; nothing changes on an error
}

std::string errorJson(const std::string& message) {
  JsonWriter j;
  j.beginObject().key("error").value(message).endObject();
  return j.str();
}

std::string stateJson(const SettingsManager& settings, const DeviceStatus& status) {
  const LightSettings& s = settings.current();
  JsonWriter j;
  j.beginObject();
  j.key("revision").value(settings.revision());
  j.key("on").value(s.on);
  j.key("mode").value(modeName(s.mode));
  j.key("brightness").value(static_cast<int>(s.brightness));
  j.key("colourA").value(hex(s.colourA));
  j.key("colourB").value(hex(s.colourB));
  j.key("spectrumColour").value(spectrumColourName(s.spectrumColour));

  j.key("modes").beginArray();
  for (uint8_t m = 0; m < kModeCount; ++m) j.value(modeName(static_cast<Mode>(m)));
  j.endArray();

  j.key("presets").beginArray();
  for (int slot = 0; slot < SettingsManager::kPresetCount; ++slot) {
    j.beginObject();
    j.key("slot").value(slot + 1);
    j.key("name").value(settings.presetName(slot));
    j.key("used").value(settings.presetUsed(slot));
    if (const LightSettings* p = settings.preset(slot)) {
      j.key("mode").value(modeName(p->mode));
      j.key("colourA").value(hex(p->colourA));
      j.key("colourB").value(hex(p->colourB));
      j.key("spectrumColour").value(spectrumColourName(p->spectrumColour));
    }
    j.endObject();
  }
  j.endArray();

  // Strip setup: as chosen, what's running, and the limits for the page's checks.
  j.key("strip").beginObject();
  j.key("ledCount").value(static_cast<int>(status.stripChosen.ledCount));
  j.key("bassEnd").value(status.stripChosen.bassOnRight ? "right" : "left");
  j.key("lowHz").value(static_cast<int>(status.stripChosen.lowHz + 0.5f));
  j.key("highHz").value(static_cast<int>(status.stripChosen.highHz + 0.5f));
  j.key("runningLedCount").value(static_cast<int>(status.stripRunning.ledCount));
  j.key("fellBack").value(status.stripFellBack);
  j.key("limits").beginObject();
  j.key("minLeds").value(config::kMinLedCount);
  j.key("maxLeds").value(config::kMaxLedCount);
  j.key("lowestHz").value(static_cast<int>(config::kLowestHz));
  j.key("highestHz").value(static_cast<int>(config::kHighestHz));
  j.key("loadLimitPercent").value(static_cast<int>(config::kAudioLoadLimit * 100 + 0.5f));
  j.endObject();
  j.endObject();

  j.key("status").beginObject();
  j.key("version").value(status.version);
  j.key("uptime").value(status.uptimeSeconds);
  j.key("wifi").beginObject();
  j.key("connected").value(status.wifiConnected);
  j.key("ssid").value(status.ssid);
  j.key("rssi").value(status.rssi);
  j.key("ip").value(status.ip);
  j.key("url").value(status.ip.empty() ? std::string() : "http://" + status.ip + "/");
  j.key("localUrl").value("http://" + status.hostname + ".local/");
  j.endObject();
  j.key("audio").beginObject();
  j.key("ok").value(status.audioOk);
  j.key("silent").value(status.silent);
  j.key("learningNoise").value(status.learningNoise);
  j.key("loadPercent").value(status.audioLoadPercent);
  j.endObject();
  j.key("update").beginObject();
  j.key("maxBytes").value(status.updateMaxBytes);
  j.key("onTrial").value(status.updateOnTrial);
  j.key("lastUndone").value(status.updateLastUndone);
  j.endObject();
  j.key("memory").beginObject();
  j.key("free").value(status.heapFree);
  j.key("lowest").value(status.heapLowest);
  j.endObject();
  j.key("smartHome").beginObject();
  j.key("started").value(status.homeStarted);
  j.key("homes").beginArray();
  for (const std::string& app : status.homeApps) j.value(app);
  j.endArray();
  j.key("pairingOpen").value(status.homePairingOpen);
  // The codes let anyone pair while pairing is open, so they are only shown then.
  j.key("code").value(status.homePairingOpen ? status.homeManualCode : std::string());
  j.key("qr").value(status.homePairingOpen ? status.homeQrPayload : std::string());
  j.endObject();
  j.endObject();

  j.endObject();
  return j.str();
}

std::string applyState(const FormArgs& args, SettingsManager& settings, uint32_t nowMillis) {
  LightSettings next = settings.current();
  if (const char* v = args.get("on")) {
    if (!parseBool(v, &next.on)) return "'on' must be 1 or 0";
  }
  if (const char* v = args.get("mode")) {
    if (!modeFromName(v, &next.mode)) return std::string("unknown mode '") + v + "'";
  }
  if (const char* v = args.get("brightness")) {
    int b;
    if (!parseInt(v, 0, 255, &b)) return "'brightness' must be 0-255";
    next.brightness = static_cast<uint8_t>(b);
  }
  if (const char* v = args.get("colourA")) {
    if (!parseHexColour(v, &next.colourA)) return "'colourA' must be #RRGGBB";
  }
  if (const char* v = args.get("colourB")) {
    if (!parseHexColour(v, &next.colourB)) return "'colourB' must be #RRGGBB";
  }
  if (const char* v = args.get("spectrumColour")) {
    if (!spectrumColourFromName(v, &next.spectrumColour))
      return "'spectrumColour' must be rainbow or fixed";
  }
  settings.update(next, nowMillis);
  return "";
}

std::string presetAction(const char* action, const FormArgs& args, SettingsManager& settings,
                         uint32_t nowMillis) {
  int slot;
  if (!parseSlot(args, &slot)) return "'slot' must be 1-8";

  if (std::strcmp(action, "save") == 0) {
    return settings.savePreset(slot, args.get("name")) ? "" : "could not save the preset";
  }
  if (std::strcmp(action, "recall") == 0) {
    return settings.recallPreset(slot, nowMillis) ? "" : "that preset is empty";
  }
  if (std::strcmp(action, "rename") == 0) {
    if (args.get("name") == nullptr) return "'name' is required";
    return settings.renamePreset(slot, args.get("name")) ? "" : "that preset is empty";
  }
  if (std::strcmp(action, "delete") == 0) {
    return settings.deletePreset(slot) ? "" : "could not delete the preset";
  }
  return std::string("unknown preset action '") + action + "'";
}

}  // namespace web_api
