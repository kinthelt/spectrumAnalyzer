#include "MatterBridge.h"

#include <app/server/Server.h>
#include <esp_attr.h>

#include "../config/Config.h"
#include "../util/Log.h"
#include "../util/SystemInfo.h"

// Survives restarts and crashes, not a power cut (see CrashGuard).
RTC_NOINIT_ATTR static CrashGuard::State sCrashGuardState;

namespace {

bool openWindowLocked() {
  chip::CommissioningWindowManager& mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
  if (mgr.IsCommissioningWindowOpen()) return true;
  const CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(config::kPairingWindowSeconds),
                                                          chip::CommissioningWindowAdvertisement::kDnssdOnly);
  if (err != CHIP_NO_ERROR) {
    LOG_ERROR("home", "could not open pairing (error %" CHIP_ERROR_FORMAT ")", err.Format());
    return false;
  }
  return true;
}

}  // namespace

MatterBridge::MatterBridge(SettingsManager& settings) : settings_(settings), guard_(sCrashGuardState) {}

void MatterBridge::begin(bool crashed) {
  guard_.begin(crashed);
  status_.paused = !guard_.allowed();
  if (status_.paused) {
    LOG_WARN("home", "Apple/Google Home paused: the lights crashed %d times in a row just after starting it "
                     "(turn it back on from the web page's Settings, or switch off at the plug)",
             guard_.crashes());
  }
}

void MatterBridge::resume() {
  guard_.reset();
  LOG_INFO("home", "Apple/Google Home will start again after the restart");
}

void MatterBridge::start() {
  loopTask_ = xTaskGetCurrentTaskHandle();
  logMemory("before Matter");
  const LightSettings& s = settings_.current();

  // Home's starting values are the lights' saved settings.
  view_.on = s.on;
  view_.level = home::brightnessToLevel(s.brightness);
  view_.colour = s.colourA;
  home::rgbToHueSat(s.colourA, &view_.hue, &view_.saturation);
  light_.begin(view_.on, espHsvColor_t{view_.hue, view_.saturation, view_.level}, view_.level);

  // Callbacks run on Matter's task: only post to the inbox. Returning true accepts the change.
  light_.onChangeOnOff([this](bool on) {
    if (!isOwnEcho()) inbox_.postOn(on);
    return true;
  });
  light_.onChangeBrightness([this](uint8_t level) {
    if (!isOwnEcho()) inbox_.postLevel(level);
    return true;
  });
  light_.onChangeColorHSV([this](espHsvColor_t hsv) {
    if (!isOwnEcho()) inbox_.postHueSat(static_cast<uint8_t>(hsv.h > 254 ? 254 : hsv.h), hsv.s);
    return true;
  });
  light_.onChangeColorTemperature([this](uint16_t mireds) {
    if (!isOwnEcho()) inbox_.postTemperature(mireds);
    return true;
  });

  Matter.setBLECommissioningEnabled(false);   // pair over Wi-Fi (step 6 spike)
  Matter.setProductName(config::kSmartHomeName);
  Matter.setDeviceName(config::kSmartHomeName);
  Matter.setSoftwareVersionString(config::kFirmwareVersion);
  guard_.starting();   // a crash from here until stable() counts against Matter
  Matter.begin();
  started_ = true;
  startMillis_ = millis();
  logMemory("Matter started");

  status_.started = true;
  status_.manualCode = Matter.getManualPairingCode().c_str();
  status_.qrPayload = home::qrPayloadFromUrl(Matter.getOnboardingQRCodeUrl().c_str());
  refreshStatus();
  LOG_INFO("home", "Matter started; %s", status_.homes.empty() ? "not paired yet" : "paired");
  if (status_.homes.empty()) LOG_INFO("home", "pairing code %s (also on the web page's Settings)", status_.manualCode.c_str());
}

void MatterBridge::loop(bool wifiConnected, uint32_t nowMillis) {
  if (!started_) {
    if (status_.paused || !wifiConnected) return;
    start();
  }
  // millis(), not nowMillis: nowMillis was read before start() above, so on the
  // first loop it is earlier than startMillis_ and the difference wrapped round
  // to a huge number (bench: "running 60 s" was logged at once).
  if (!stableNoted_ && millis() - startMillis_ >= config::kMatterStableMillis) {
    stableNoted_ = true;
    guard_.stable();
    logMemory("Matter running 60 s");
  }

  // From Home: apply what arrived since the last loop.
  const home::HomeChanges changes = inbox_.take();
  if (!changes.empty() && home::applyHomeChanges(changes, settings_, &view_, nowMillis)) {
    LOG_INFO("home", "from Home:%s%s%s", changes.hasOn ? (changes.on ? " on" : " off") : "",
             changes.hasLevel ? " brightness" : "",
             changes.colour == home::HomeChanges::Colour::None ? ""
             : changes.colour == home::HomeChanges::Colour::Temperature ? " white (Solid)" : " colour (Solid)");
  }

  // To Home: changes made elsewhere (web page, presets, console).
  if (nowMillis - lastSendMillis_ >= config::kHomeSendIntervalMillis) {
    lastSendMillis_ = nowMillis;
    sendToHome(nowMillis);
  }

  if (nowMillis - lastStatusMillis_ >= 1000) {
    lastStatusMillis_ = nowMillis;
    refreshStatus();
  }
}

void MatterBridge::sendToHome(uint32_t nowMillis) {
  (void)nowMillis;
  const home::HomeUpdate u = home::updateForHome(settings_.current(), &view_);
  if (!u.sendOn && !u.sendLevel && !u.sendColour) return;
  sending_.store(true);   // the library may call our callbacks from inside these: echoes
  if (u.sendOn) light_.setOnOff(view_.on);
  if (u.sendLevel) light_.setBrightness(view_.level);
  if (u.sendColour) light_.setColorHSV(espHsvColor_t{view_.hue, view_.saturation, view_.level});
  sending_.store(false);
}

void MatterBridge::refreshStatus() {
  esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
  status_.homes.clear();
  for (const chip::FabricInfo& fabric : chip::Server::GetInstance().GetFabricTable()) {
    const chip::CharSpan label = fabric.GetFabricLabel();
    status_.homes.push_back(home::homeAppName(static_cast<uint16_t>(fabric.GetVendorId()),
                                              std::string(label.data(), label.size())));
  }
  status_.pairingOpen = chip::Server::GetInstance().GetCommissioningWindowManager().IsCommissioningWindowOpen();
}

bool MatterBridge::openPairing() {
  if (!started_) return false;
  bool ok;
  {
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    ok = openWindowLocked();
  }
  if (ok) LOG_INFO("home", "pairing open for %lu min", (unsigned long)(config::kPairingWindowSeconds / 60));
  refreshStatus();
  return ok;
}

bool MatterBridge::removeAllHomes() {
  if (!started_) return false;
  bool ok;
  {
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    chip::FabricTable& table = chip::Server::GetInstance().GetFabricTable();
    std::vector<chip::FabricIndex> indexes;
    for (const chip::FabricInfo& fabric : table) indexes.push_back(fabric.GetFabricIndex());
    for (chip::FabricIndex i : indexes) {
      const CHIP_ERROR err = table.Delete(i);
      if (err != CHIP_NO_ERROR) LOG_ERROR("home", "could not remove pairing %u (error %" CHIP_ERROR_FORMAT ")", i, err.Format());
    }
    LOG_INFO("home", "removed %u pairing(s); Wi-Fi and settings kept", (unsigned)indexes.size());
    ok = table.FabricCount() == 0 && openWindowLocked();
  }
  refreshStatus();
  return ok;
}
