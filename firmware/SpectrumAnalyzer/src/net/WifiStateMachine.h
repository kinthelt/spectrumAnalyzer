// Decides when to connect to the saved Wi-Fi and when to open the setup
// network ("Spectrum-Setup"). Plain C++: it is told what is happening and says
// what to do, so the rules are unit-tested on a PC. WifiSetup carries out the
// actions with WiFiManager.
//
//   Connecting   --connected-->                       Connected
//   Connecting   --no connection for 2 min-->         SetupPortal
//   Connected    --lost for 5 min-->                  SetupPortal
//   SetupPortal  --someone saves working Wi-Fi-->     Connected
//   SetupPortal  --10 min, a network is saved-->      Connecting (try it again)
//   any state    --"forget Wi-Fi"-->                  SetupPortal
#pragma once

#include <cstdint>

class WifiStateMachine {
 public:
  enum class State : uint8_t { Connecting, Connected, SetupPortal };
  enum class Action : uint8_t { None, Connect, StartPortal, StopPortalAndConnect };

  // Decides the first action. forceSetup: open setup regardless (power-cycle trick).
  Action begin(bool hasSavedNetwork, bool forceSetup, uint32_t nowMillis);

  // Call every loop. connected: the station has an IP address.
  Action update(bool hasSavedNetwork, bool connected, uint32_t nowMillis);

  // The saved network was erased ("forget Wi-Fi").
  Action forget(uint32_t nowMillis);

  State state() const { return state_; }
  static const char* stateName(State s);

 private:
  Action enter(State s, uint32_t nowMillis);

  State state_ = State::Connecting;
  uint32_t stateSinceMillis_ = 0;
  bool lost_ = false;
  uint32_t lostSinceMillis_ = 0;
};
