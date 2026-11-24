// The web page's API, as plain functions: build the state JSON, apply changes.
// The HTTP side (WebUi) only moves text in and out, so everything that decides
// what a request means is plain C++ and unit-tested on a PC.
//
//   GET  /api/state            -> stateJson()
//   POST /api/state            on, mode, brightness, colourA, colourB, spectrumColour
//   POST /api/preset/<action>  save (slot, name?), recall (slot), rename (slot, name), delete (slot)
//
// POST bodies are form fields (application/x-www-form-urlencoded). Changes are
// all or nothing: if any field is invalid, nothing changes and the error names it.
#pragma once

#include <cstdint>
#include <string>

#include "../settings/SettingsManager.h"

// Request fields, looked up by name. nullptr if absent.
class FormArgs {
 public:
  virtual ~FormArgs() = default;
  virtual const char* get(const char* name) const = 0;
};

// Read-only facts about the device for the status panel (filled by WebUi).
struct DeviceStatus {
  std::string version;
  bool wifiConnected = false;
  std::string ssid;
  int32_t rssi = 0;
  std::string ip;        // "" when not connected
  std::string hostname;  // "spectrum" -> spectrum.local
  bool audioOk = false;
  bool silent = true;
  bool learningNoise = false;
  int32_t audioLoadPercent = 0;
  uint32_t uptimeSeconds = 0;
};

namespace web_api {

std::string stateJson(const SettingsManager& settings, const DeviceStatus& status);

// Both return "" on success, otherwise an error message for the page to show.
std::string applyState(const FormArgs& args, SettingsManager& settings, uint32_t nowMillis);
std::string presetAction(const char* action, const FormArgs& args, SettingsManager& settings,
                         uint32_t nowMillis);

std::string errorJson(const std::string& message);

}  // namespace web_api
