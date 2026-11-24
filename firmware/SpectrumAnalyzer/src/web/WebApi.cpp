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
