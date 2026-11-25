// The web page's API, as plain functions: build the state JSON, apply changes.
// The HTTP side (WebUi) only moves text in and out, so everything that decides
// what a request means is plain C++ and unit-tested on a PC.
//
//   GET  /api/state            -> stateJson()
//   POST /api/state            on, mode, brightness, colourA, colourB, spectrumColour
//   POST /api/preset/<action>  save (slot, name?), recall (slot), rename (slot, name), delete (slot)
//   POST /api/strip            ledCount, bassEnd (left|right)
//   (POST /api/action/<name> and POST /api/update are handled in WebUi.)
//
// POST bodies are form fields (application/x-www-form-urlencoded). Changes are
// all or nothing: if any field is invalid, nothing changes and the error names it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../settings/SettingsManager.h"
#include "../settings/StripSetupManager.h"

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
  uint32_t updateMaxBytes = 0;    // largest firmware file the spare slot takes
  bool updateOnTrial = false;     // just updated; not yet declared good
  bool updateLastUndone = false;  // the last update was undone (previous firmware came back)
  // Apple Home / Google Home (Matter)
  bool homeStarted = false;              // Matter runs once the lights are on Wi-Fi
  bool homePaused = false;               // skipped after crashing the lights (CrashGuard)
  std::vector<std::string> homeApps;     // apps it is paired with
  bool homePairingOpen = false;
  std::string homeManualCode;            // only sent to the page while pairing is open
  std::string homeQrPayload;             // "MT:...", likewise
  // Strip setup (Settings > Strip setup)
  StripSetup stripChosen;                // as set on the page
  StripSetup stripRunning;               // what the lights are using
  // Memory: internal RAM free now, and the lowest it has been since start-up.
  uint32_t heapFree = 0;
  uint32_t heapLowest = 0;
};

namespace web_api {

std::string stateJson(const SettingsManager& settings, const DeviceStatus& status);

// Both return "" on success, otherwise an error message for the page to show.
std::string applyState(const FormArgs& args, SettingsManager& settings, uint32_t nowMillis);
std::string presetAction(const char* action, const FormArgs& args, SettingsManager& settings,
                         uint32_t nowMillis);

// Changes the strip setup (fields present only; all or nothing). needsRestart:
// the lights must restart to use it (the LED count changed).
std::string applyStrip(const FormArgs& args, StripSetupManager& strip, bool* needsRestart);

std::string errorJson(const std::string& message);

}  // namespace web_api
