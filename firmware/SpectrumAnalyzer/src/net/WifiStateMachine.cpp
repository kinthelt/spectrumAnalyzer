#include "WifiStateMachine.h"

#include "../config/Config.h"

const char* WifiStateMachine::stateName(State s) {
  switch (s) {
    case State::Connecting: return "connecting";
    case State::Connected: return "connected";
    case State::SetupPortal: return "setup network open";
  }
  return "?";
}

WifiStateMachine::Action WifiStateMachine::enter(State s, uint32_t nowMillis) {
  const State previous = state_;
  state_ = s;
  stateSinceMillis_ = nowMillis;
  lost_ = false;
  switch (s) {
    case State::Connecting:
      return previous == State::SetupPortal ? Action::StopPortalAndConnect : Action::Connect;
    case State::SetupPortal: return Action::StartPortal;
    case State::Connected: return Action::None;
  }
  return Action::None;
}

WifiStateMachine::Action WifiStateMachine::begin(bool hasSavedNetwork, bool forceSetup,
                                                 uint32_t nowMillis) {
  state_ = State::Connecting;   // so entering Connecting below asks for a plain Connect
  if (forceSetup || !hasSavedNetwork) return enter(State::SetupPortal, nowMillis);
  return enter(State::Connecting, nowMillis);
}

WifiStateMachine::Action WifiStateMachine::update(bool hasSavedNetwork, bool connected,
                                                  uint32_t nowMillis) {
  const uint32_t inState = nowMillis - stateSinceMillis_;
  switch (state_) {
    case State::Connecting:
      if (connected) return enter(State::Connected, nowMillis);
      if (inState >= config::kWifiConnectTimeoutMillis) return enter(State::SetupPortal, nowMillis);
      return Action::None;

    case State::Connected:
      if (connected) {
        lost_ = false;
        return Action::None;
      }
      if (!lost_) {
        lost_ = true;
        lostSinceMillis_ = nowMillis;
      }
      // Meanwhile the Wi-Fi driver keeps retrying by itself.
      if (nowMillis - lostSinceMillis_ >= config::kWifiLostBeforeSetupMillis) {
        return enter(State::SetupPortal, nowMillis);
      }
      return Action::None;

    case State::SetupPortal:
      if (connected) return enter(State::Connected, nowMillis);
      if (inState >= config::kSetupPortalMillis) {
        // With a saved network, close the setup network and try it again (the
        // router may simply have been off). With none, keep setup open.
        if (hasSavedNetwork) return enter(State::Connecting, nowMillis);
        stateSinceMillis_ = nowMillis;
      }
      return Action::None;
  }
  return Action::None;
}

WifiStateMachine::Action WifiStateMachine::forget(uint32_t nowMillis) {
  if (state_ == State::SetupPortal) {
    stateSinceMillis_ = nowMillis;
    return Action::None;
  }
  return enter(State::SetupPortal, nowMillis);
}
