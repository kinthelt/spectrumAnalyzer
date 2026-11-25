// Firmware updates over Wi-Fi, on the board: receiving the file (the web page's
// POST /api/update, also used by tools/build.sh ota), showing progress on the
// strip, restarting into the new firmware, and deciding whether that firmware
// is good or the previous one should come back.
//
// The rules live in plain C++ (UpdateSession, BootValidator) and are unit-tested;
// this class connects them to the Update library and ESP-IDF's OTA functions.
// Everything here runs from the Arduino loop (core 1). The LED task only reads
// progress(), a shared copy.
#pragma once

#include <functional>
#include <string>

#include "../util/Shared.h"
#include "BootValidator.h"
#include "UpdateProgress.h"
#include "UpdateSession.h"

class UpdateWriter : public FirmwareWriter {
 public:
  bool begin() override;
  bool write(const uint8_t* data, size_t len) override;
  bool finish() override;
  void abort() override;
  std::string error() const override;
};

class FirmwareUpdate {
 public:
  FirmwareUpdate();

  // Call once early in setup(): finds out whether this firmware is on trial
  // (just installed) and whether the last update was undone.
  void begin(uint32_t nowMillis);

  // Called just before restarting into new firmware (to save settings).
  void setBeforeRestart(std::function<void()> fn) { beforeRestart_ = fn; }

  // Called with true when an upload starts and false if it fails (after success
  // the board restarts). Used to pause the audio analysis: writing flash freezes
  // core 0 for about 45 ms per 4 KB, and together with the analysis's 43% that
  // starved core 0 until the task watchdog restarted the board (bench, step 8).
  void setPauseHook(std::function<void(bool)> fn) { pauseHook_ = fn; }

  // ---- The upload, from WebUi ----
  // contentLength: the request's length (a little more than the file). Returns
  // "" if accepted, or why not.
  std::string uploadStart(size_t contentLength, uint32_t nowMillis);
  void uploadData(const uint8_t* data, size_t len, uint32_t nowMillis);
  void uploadEnd(uint32_t nowMillis);
  void uploadAborted(uint32_t nowMillis);
  bool succeeded() const { return session_.phase() == UpdateSession::Phase::Succeeded; }
  // An update is being received, or has been installed and the restart is due.
  bool busy() const { return session_.phase() == UpdateSession::Phase::Receiving || succeeded(); }
  const std::string& error() const { return session_.error(); }

  // healthy: on the home Wi-Fi with the web page running. Call every loop.
  void loop(bool healthy, uint32_t nowMillis);

  const Shared<UpdateProgress>& progress() const { return progress_; }
  size_t maxBytes() const { return session_.maxBytes(); }
  bool onTrial() const { return validator_.onTrial(); }
  // The last update didn't prove itself, so the board went back to this firmware.
  bool lastUpdateUndone() const { return lastUndone_; }

 private:
  void publish(uint32_t nowMillis);
  // One log line about how the upload went, to tell a Wi-Fi stall (a long gap,
  // the last piece ~5 s ago) from a dropped connection (last piece just now).
  void logTransfer(const char* outcome, uint32_t nowMillis);

  UpdateWriter writer_;
  UpdateSession session_;
  BootValidator validator_;
  Shared<UpdateProgress> progress_;
  UpdateProgress published_;
  std::function<void()> beforeRestart_;
  std::function<void(bool)> pauseHook_;
  bool othersPaused_ = false;
  bool lastUndone_ = false;
  // Timing of the current upload, for logTransfer().
  uint32_t uploadStartMillis_ = 0;
  uint32_t lastPieceMillis_ = 0;
  uint32_t longestGapMillis_ = 0;
};
