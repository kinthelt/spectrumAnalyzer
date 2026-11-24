// Everything Wi-Fi: connecting to the saved network, the "Spectrum-Setup"
// network for entering Wi-Fi details, the spectrum.local name and the status page.
// Runs from the Arduino loop on core 1 and never blocks, so the LEDs and audio
// keep running while the setup network is open.
#pragma once

#include <WiFiManager.h>

#include "LocalName.h"
#include "StatusPage.h"
#include "WifiStateMachine.h"

class Network {
 public:
  // forceSetup: open the setup network at once (power-cycle trick).
  void begin(bool forceSetup);
  void loop(uint32_t nowMillis);

  // Erases the saved Wi-Fi and opens the setup network.
  void forgetWifi();

  // Text for the status page (supplied by the sketch, which knows the app state).
  void setStatusDescriber(StatusPage::Describer describe) { page_.setDescriber(describe); }

  // One-line summary for Serial Monitor.
  String summary();

 private:
  void apply(WifiStateMachine::Action action);

  WiFiManager manager_;
  WifiStateMachine machine_;
  LocalName name_;
  StatusPage page_;
  WifiStateMachine::State lastLoggedState_ = WifiStateMachine::State::Connecting;
  bool portalOpen_ = false;
  bool savedNetwork_ = false;          // cached; refreshed once a second
  uint32_t savedCheckedMillis_ = 0;
};
