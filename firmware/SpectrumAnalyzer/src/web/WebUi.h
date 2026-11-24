// The web page and its API on port 80 (http://spectrum.local).
//
//   GET  /                     the page (web/index.html, gzipped, from WebPage.h)
//   GET  /api/state            everything the page shows (WebApi::stateJson)
//   POST /api/state            change on, mode, brightness, colours
//   POST /api/preset/<action>  save | recall | rename | delete
//   POST /api/action/<name>    learn-noise | forget-wifi | reset-settings
//
// This class is only the HTTP glue: what a request means is decided in WebApi,
// which is unit-tested on a PC. It runs from the Arduino loop (core 1), the
// same place that owns the settings, so no locking is needed.
#pragma once

#include <WebServer.h>

#include <functional>

#include "../net/HttpService.h"
#include "../settings/SettingsManager.h"
#include "WebApi.h"

class WebUi : public HttpService {
 public:
  // What the page needs from the rest of the firmware.
  struct Hooks {
    std::function<DeviceStatus()> status;   // facts for the status panel
    std::function<void()> learnNoise;       // start learning the room's noise
    std::function<void()> forgetWifi;       // erase Wi-Fi and open the setup network
  };

  WebUi(SettingsManager& settings, Hooks hooks) : settings_(settings), hooks_(hooks) {}

  void start() override;
  void stop() override;
  void loop() override;

 private:
  void addRoutes();
  void sendPage();
  void sendState(int code = 200);
  void sendError(int code, const std::string& message);
  void handleState();
  void handlePreset(const char* action);
  void handleAction(const char* action);

  WebServer server_{80};
  SettingsManager& settings_;
  Hooks hooks_;
  bool routesAdded_ = false;
  bool running_ = false;
  // Forget Wi-Fi waits until the reply has reached the phone; this is when it was asked.
  uint32_t forgetRequestedMillis_ = 0;
  bool forgetPending_ = false;
};
