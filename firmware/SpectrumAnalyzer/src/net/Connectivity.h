// Everything Wi-Fi (named Connectivity because Arduino-ESP32 already has a
// global object called Network): connecting to the saved network, the "Spectrum-Setup"
// network for entering Wi-Fi details, the spectrum.local name, and when the web page may use port 80.
// Runs from the Arduino loop on core 1 and never blocks, so the LEDs and audio
// keep running while the setup network is open.
#pragma once

#include <WiFiManager.h>

#include "HttpService.h"
#include "LocalName.h"
#include "WifiStateMachine.h"

class Connectivity {
 public:
  // forceSetup: open the setup network at once (power-cycle trick).
  void begin(bool forceSetup);
  void loop(uint32_t nowMillis);

  // Erases the saved Wi-Fi and opens the setup network.
  void forgetWifi();

  // The web page: started while on the home Wi-Fi, stopped while the setup
  // network is open. Set before begin(); may be left unset.
  void setWebService(HttpService* web) { web_ = web; }

  bool connected() const { return connected_; }

  // One-line summary for Serial Monitor.
  String summary();

 private:
  void apply(WifiStateMachine::Action action);
  void stopWeb() {
    if (web_ != nullptr) web_->stop();
  }

  WiFiManager manager_;
  WifiStateMachine machine_;
  LocalName name_;
  HttpService* web_ = nullptr;
  bool connected_ = false;
  WifiStateMachine::State lastLoggedState_ = WifiStateMachine::State::Connecting;
  bool portalOpen_ = false;
  bool savedNetwork_ = false;          // cached; refreshed once a second
  uint32_t savedCheckedMillis_ = 0;
};
