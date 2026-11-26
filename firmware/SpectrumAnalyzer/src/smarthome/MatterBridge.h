// The lights in Apple Home and Google Home, through Matter.
//
// The strip is one Matter light with colour and white temperature
// (MatterEnhancedColorLight). HomeRules decides what changes mean; this class
// connects those rules to the Matter library:
//   - Matter starts once the lights are first on the home Wi-Fi (pairing is over
//     Wi-Fi, never Bluetooth), the order proven in the step 6 spike;
//   - Home's changes arrive on Matter's own task and are posted to a HomeInbox;
//     loop() applies them to the settings (the loop is the settings' only writer);
//   - loop() sends setting changes made elsewhere back to Home, at most every
//     kHomeSendIntervalMillis. The library reports some of our own sends back
//     through the same callbacks; those echoes are recognised (they arrive on the
//     loop's own task while it is sending) and ignored.
//   - pairing: status for the Settings page, opening pairing for
//     kPairingWindowSeconds, and removing every pairing (Wi-Fi and settings kept);
//   - a crash-loop safety net (CrashGuard): crashes right after Matter starts
//     pause it, so the lights stay reachable.
#pragma once

#include <Arduino.h>
#include <Matter.h>

#include <atomic>
#include <string>
#include <vector>

#include "../config/Config.h"
#include "../settings/SettingsManager.h"
#include "CrashGuard.h"
#include "HomeRules.h"

struct SmartHomeStatus {
  bool started = false;               // Matter is running (after the first Wi-Fi connection)
  bool paused = false;                // skipped: it crashed the lights repeatedly (CrashGuard)
  std::vector<std::string> homes;     // apps it is paired with; empty = not paired
  bool pairingOpen = false;           // a Home app can pair now
  std::string manualCode;             // 11 digits, for typing in
  std::string qrPayload;              // "MT:...", the text in the QR code
};

class MatterBridge {
 public:
  explicit MatterBridge(SettingsManager& settings);

  // Call once in setup(). crashed: the last reset was a crash or watchdog.
  void begin(bool crashed);

  // Call every loop. wifiConnected: on the home Wi-Fi (starts Matter the first time).
  void loop(bool wifiConnected, uint32_t nowMillis);

  const SmartHomeStatus& status() const { return status_; }

  // Matter has started, and its start-up memory spike is over (see kMatterSettleMillis).
  bool started() const { return started_; }
  bool settled() const {
    return started_ && (bleMemoryBack_.load() || millis() - startMillis_ >= config::kMatterSettleMillis);
  }

  // Lets a Home app pair for kPairingWindowSeconds. Returns false if it couldn't.
  bool openPairing();
  // Removes every pairing, then opens pairing so the lights can be added again.
  bool removeAllHomes();
  // After a pause: Matter may start again from the next start-up (the caller restarts).
  void resume();

 private:
  void start();
  void sendToHome(uint32_t nowMillis);
  void refreshStatus();
  bool isOwnEcho() const { return sending_.load() && xTaskGetCurrentTaskHandle() == loopTask_; }

  SettingsManager& settings_;
  CrashGuard guard_;
  uint32_t startMillis_ = 0;
  bool stableNoted_ = false;
  MatterEnhancedColorLight light_;
  home::HomeInbox inbox_;
  home::HomeView view_;            // what Home shows (as last sent or received)
  std::atomic<bool> sending_{false};
  std::atomic<bool> bleMemoryBack_{false};   // set on Matter's task
  TaskHandle_t loopTask_ = nullptr;
  bool started_ = false;
  uint32_t lastSendMillis_ = 0;
  uint32_t lastStatusMillis_ = 0;
  SmartHomeStatus status_;
};
