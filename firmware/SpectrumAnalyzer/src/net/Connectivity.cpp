#include "Connectivity.h"

#include <WiFi.h>

#include "../config/Config.h"
#include "../util/Log.h"
#include "StationIp.h"

void Connectivity::begin(bool forceSetup) {
  WiFi.setHostname(config::kHostname);   // name shown in the router's device list
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  manager_.setConfigPortalBlocking(false);   // LEDs and audio keep running
  manager_.setConfigPortalTimeout(0);        // WifiStateMachine decides when to close it
  manager_.setTitle("Spectrum Analyzer");
  manager_.setHostname(config::kHostname);
  static const char* menu[] = {"wifi", "exit"};
  manager_.setMenu(menu, 2);

  const bool saved = manager_.getWiFiIsSaved();
  savedNetwork_ = saved;
  if (forceSetup) LOG_INFO("wifi", "power-cycle sequence detected: opening the setup network");
  apply(machine_.begin(saved, forceSetup, millis()));
  lastLoggedState_ = machine_.state();
}

void Connectivity::apply(WifiStateMachine::Action action) {
  switch (action) {
    case WifiStateMachine::Action::None:
      break;
    case WifiStateMachine::Action::Connect:
      LOG_INFO("wifi", "connecting to saved network '%s'", WiFi.SSID().c_str());
      WiFi.begin();   // the saved network
      break;
    case WifiStateMachine::Action::StartPortal:
      page_.stop();   // the setup page needs port 80
      manager_.startConfigPortal(config::kSetupNetworkName);
      portalOpen_ = true;
      LOG_INFO("wifi", "setup network '%s' open: join it on a phone to enter Wi-Fi details",
               config::kSetupNetworkName);
      break;
    case WifiStateMachine::Action::StopPortalAndConnect:
      manager_.stopConfigPortal();
      portalOpen_ = false;
      WiFi.mode(WIFI_STA);
      LOG_INFO("wifi", "closing the setup network, trying the saved network again");
      WiFi.begin();
      break;
  }
}

void Connectivity::loop(uint32_t nowMillis) {
  if (portalOpen_) manager_.process();

  if (nowMillis - savedCheckedMillis_ >= 1000) {
    savedCheckedMillis_ = nowMillis;
    savedNetwork_ = manager_.getWiFiIsSaved();
  }

  uint32_t ip = 0;
  const bool hasIp = stationIp(&ip);
  apply(machine_.update(savedNetwork_, hasIp, nowMillis));

  if (machine_.state() != WifiStateMachine::State::SetupPortal && portalOpen_) {
    manager_.stopConfigPortal();   // connected through the setup page
    portalOpen_ = false;
  }

  if (machine_.state() != lastLoggedState_) {
    lastLoggedState_ = machine_.state();
    LOG_INFO("wifi", "%s", summary().c_str());
  }

  name_.update(hasIp, ip);

  // Status page whenever connected and the setup page isn't using port 80.
  if (hasIp && !portalOpen_) {
    page_.start();
  } else {
    page_.stop();
  }
  page_.loop();
}

void Connectivity::forgetWifi() {
  LOG_INFO("wifi", "forgetting the saved Wi-Fi");
  page_.stop();
  manager_.resetSettings();   // erases the saved network
  savedNetwork_ = false;
  apply(machine_.forget(millis()));
}

String Connectivity::summary() {
  uint32_t ip = 0;
  String s = WifiStateMachine::stateName(machine_.state());
  if (stationIp(&ip)) {
    s += ", network '" + WiFi.SSID() + "', signal " + String(WiFi.RSSI()) + " dBm, http://" +
         config::kHostname + ".local (" + ipToString(ip) + ")";
  } else if (portalOpen_) {
    s += String(", join '") + config::kSetupNetworkName + "' on a phone";
  }
  return s;
}
