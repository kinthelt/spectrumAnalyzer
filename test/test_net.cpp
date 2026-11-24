// Unit tests for src/net: the Wi-Fi state machine and the power-cycle counter.

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "config/Config.h"
#include "net/PowerCycleCounter.h"
#include "net/WifiStateMachine.h"

using WSM = WifiStateMachine;

namespace {
constexpr uint32_t kMin = 60u * 1000u;
}

// ---- Wi-Fi state machine -----------------------------------------------------------

TEST(wifi_first_boot_without_saved_network_opens_setup) {
  WSM m;
  CHECK(m.begin(false, false, 0) == WSM::Action::StartPortal);
  CHECK(m.state() == WSM::State::SetupPortal);
}

TEST(wifi_saved_network_connects) {
  WSM m;
  CHECK(m.begin(true, false, 0) == WSM::Action::Connect);
  CHECK(m.update(true, false, 1000) == WSM::Action::None);
  CHECK(m.update(true, true, 3000) == WSM::Action::None);
  CHECK(m.state() == WSM::State::Connected);
}

TEST(wifi_power_cycle_trick_forces_setup) {
  WSM m;
  CHECK(m.begin(true, true, 0) == WSM::Action::StartPortal);
  CHECK(m.state() == WSM::State::SetupPortal);
}

TEST(wifi_unreachable_network_opens_setup_after_two_minutes) {
  WSM m;
  m.begin(true, false, 0);
  CHECK(m.update(true, false, config::kWifiConnectTimeoutMillis - 1) == WSM::Action::None);
  CHECK(m.update(true, false, config::kWifiConnectTimeoutMillis) == WSM::Action::StartPortal);
  CHECK(m.state() == WSM::State::SetupPortal);
}

TEST(wifi_short_dropouts_are_ridden_out) {
  WSM m;
  m.begin(true, false, 0);
  m.update(true, true, 1000);
  // A 4-minute router reboot: no setup network, it just reconnects.
  for (uint32_t t = 1000; t < 1000 + 4 * kMin; t += 1000)
    CHECK(m.update(true, false, t) == WSM::Action::None);
  m.update(true, true, 1000 + 4 * kMin);
  CHECK(m.state() == WSM::State::Connected);
  // A second dropout gets its own full 5 minutes.
  for (uint32_t t = 2000 + 4 * kMin; t < 2000 + 8 * kMin; t += 1000)
    CHECK(m.update(true, false, t) == WSM::Action::None);
}

TEST(wifi_long_loss_opens_setup) {
  WSM m;
  m.begin(true, false, 0);
  m.update(true, true, 1000);
  m.update(true, false, 2000);
  CHECK(m.update(true, false, 2000 + config::kWifiLostBeforeSetupMillis) == WSM::Action::StartPortal);
}

TEST(wifi_setup_success_connects) {
  WSM m;
  m.begin(false, false, 0);
  CHECK(m.update(true, true, 30000) == WSM::Action::None);
  CHECK(m.state() == WSM::State::Connected);
}

TEST(wifi_setup_times_out_and_retries_saved_network) {
  WSM m;
  m.begin(true, true, 0);   // forced setup, but a network is saved
  CHECK(m.update(true, false, config::kSetupPortalMillis) == WSM::Action::StopPortalAndConnect);
  CHECK(m.state() == WSM::State::Connecting);
  // Still unreachable: back to setup after the connect timeout, and so on.
  CHECK(m.update(true, false, config::kSetupPortalMillis + config::kWifiConnectTimeoutMillis) ==
        WSM::Action::StartPortal);
}

TEST(wifi_setup_stays_open_with_nothing_saved) {
  WSM m;
  m.begin(false, false, 0);
  for (uint32_t t = 0; t < 3 * config::kSetupPortalMillis; t += 10000)
    CHECK(m.update(false, false, t) == WSM::Action::None);
  CHECK(m.state() == WSM::State::SetupPortal);
}

TEST(wifi_forget_opens_setup) {
  WSM m;
  m.begin(true, false, 0);
  m.update(true, true, 1000);
  CHECK(m.forget(5000) == WSM::Action::StartPortal);
  CHECK(m.state() == WSM::State::SetupPortal);
  CHECK(m.forget(6000) == WSM::Action::None);   // already open
}

// ---- Power-cycle counter --------------------------------------------------------------

TEST(power_cycles_three_quick_boots_trigger_setup) {
  FakeSettingsStore store;
  for (int boot = 1; boot < config::kPowerCyclesForSetup; ++boot) {
    PowerCycleCounter c(store);
    CHECK(!c.begin());   // switched off again before the window passes
  }
  PowerCycleCounter third(store);
  CHECK(third.begin());
  PowerCycleCounter after(store);   // count starts over afterwards
  CHECK(!after.begin());
  CHECK_EQ(after.count(), 1);
}

TEST(power_cycles_staying_on_clears_the_count) {
  FakeSettingsStore store;
  {
    PowerCycleCounter quick(store);
    CHECK(!quick.begin());   // one quick boot...
  }
  PowerCycleCounter c(store);
  CHECK(!c.begin());         // ...then a second boot that stays on
  CHECK_EQ(c.count(), 2);
  c.tick(config::kPowerCycleWindowMillis - 1);
  c.tick(config::kPowerCycleWindowMillis);   // stayed on: cleared
  PowerCycleCounter next(store);
  CHECK(!next.begin());
  CHECK_EQ(next.count(), 1);
}

TEST(power_cycles_normal_use_never_triggers) {
  FakeSettingsStore store;
  for (int day = 0; day < 30; ++day) {
    PowerCycleCounter c(store);
    CHECK(!c.begin());
    for (uint32_t t = 0; t <= 60000; t += 1000) c.tick(t);
  }
}
