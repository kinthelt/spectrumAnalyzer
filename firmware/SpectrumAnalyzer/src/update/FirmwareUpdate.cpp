#include "FirmwareUpdate.h"

#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include "../config/Config.h"
#include "../util/Log.h"

// The core's start-up code would otherwise declare new firmware good straight
// away. Returning true leaves that decision to FirmwareUpdate::loop().
// (Defined in the core's C code, hence extern "C".)
extern "C" bool verifyRollbackLater() { return true; }

namespace {

size_t spareSlotBytes() {
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  return next != nullptr ? next->size : 0;
}

UpdateProgress::Phase toProgressPhase(UpdateSession::Phase p) {
  switch (p) {
    case UpdateSession::Phase::Receiving: return UpdateProgress::Phase::Receiving;
    case UpdateSession::Phase::Succeeded: return UpdateProgress::Phase::Succeeded;
    case UpdateSession::Phase::Failed: return UpdateProgress::Phase::Failed;
    default: return UpdateProgress::Phase::Idle;
  }
}

}  // namespace

// ---- UpdateWriter: the Update library -----------------------------------------------------

bool UpdateWriter::begin() { return Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH); }

bool UpdateWriter::write(const uint8_t* data, size_t len) {
  return Update.write(const_cast<uint8_t*>(data), len) == len;
}

// end(true): the file's length is whatever arrived. end() checks the image
// (complete, checksums, built for this chip) and makes it the next to start.
bool UpdateWriter::finish() { return Update.end(true); }

void UpdateWriter::abort() {
  if (Update.isRunning()) Update.abort();
}

std::string UpdateWriter::error() const { return Update.errorString(); }

// ---- FirmwareUpdate ---------------------------------------------------------------------------

FirmwareUpdate::FirmwareUpdate()
    : session_(writer_, 0, config::kUpdateStallMillis),   // slot size is read in begin()
      validator_(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis) {}

void FirmwareUpdate::begin(uint32_t nowMillis) {
  session_.setMaxBytes(spareSlotBytes());
  esp_ota_img_states_t state;
  const bool onTrial = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
                       state == ESP_OTA_IMG_PENDING_VERIFY;
  validator_.begin(onTrial, nowMillis);
  lastUndone_ = !onTrial && esp_ota_get_last_invalid_partition() != nullptr;

  if (onTrial) {
    LOG_INFO("update", "new firmware %s on trial: good after %lu s on Wi-Fi, otherwise the previous "
                       "firmware returns after %lu s",
             config::kFirmwareVersion, (unsigned long)(config::kUpdateGoodAfterMillis / 1000),
             (unsigned long)(config::kUpdateGiveUpMillis / 1000));
  } else if (lastUndone_) {
    LOG_WARN("update", "the last update didn't prove itself, so this firmware (%s) came back",
             config::kFirmwareVersion);
  }
  if (session_.maxBytes() == 0) LOG_ERROR("update", "no spare firmware slot: check the partition scheme");
  if (config::kTestUpdateNeverConfirms) {
    LOG_WARN("update", "TEST BUILD (kTestUpdateNeverConfirms): this firmware never declares itself good");
  }
}

std::string FirmwareUpdate::uploadStart(size_t contentLength, uint32_t nowMillis) {
  const std::string err = session_.start(contentLength, nowMillis);
  if (err.empty()) {
    uploadStartMillis_ = lastPieceMillis_ = nowMillis;
    longestGapMillis_ = 0;
    LOG_INFO("update", "receiving firmware (about %u bytes)", (unsigned)contentLength);
  } else {
    LOG_WARN("update", "refused: %s", err.c_str());
  }
  publish(nowMillis);
  return err;
}

void FirmwareUpdate::uploadData(const uint8_t* data, size_t len, uint32_t nowMillis) {
  const bool wasReceiving = session_.phase() == UpdateSession::Phase::Receiving;
  if (wasReceiving) {
    const uint32_t gap = nowMillis - lastPieceMillis_;
    if (gap > longestGapMillis_) longestGapMillis_ = gap;
    lastPieceMillis_ = nowMillis;
  }
  session_.write(data, len, nowMillis);
  if (wasReceiving && session_.phase() == UpdateSession::Phase::Failed) {
    LOG_WARN("update", "failed: %s", session_.error().c_str());
    logTransfer("stopped", nowMillis);
  }
  publish(nowMillis);
}

void FirmwareUpdate::uploadEnd(uint32_t nowMillis) {
  if (session_.phase() != UpdateSession::Phase::Receiving) return;
  session_.finish(nowMillis);
  if (succeeded()) {
    logTransfer("received", nowMillis);
    LOG_INFO("update", "installed %u bytes; restarting into the new firmware", (unsigned)session_.received());
  } else {
    logTransfer("received", nowMillis);
    LOG_WARN("update", "failed: %s", session_.error().c_str());
  }
  publish(nowMillis);
}

void FirmwareUpdate::uploadAborted(uint32_t nowMillis) {
  if (session_.phase() != UpdateSession::Phase::Receiving) return;
  session_.cancel("The upload was interrupted. The lights keep their current version.", nowMillis);
  LOG_WARN("update", "upload interrupted after %u bytes", (unsigned)session_.received());
  logTransfer("interrupted", nowMillis);
  publish(nowMillis);
}

void FirmwareUpdate::loop(bool healthy, uint32_t nowMillis) {
  session_.tick(nowMillis);
  publish(nowMillis);

  if (succeeded() && nowMillis - session_.phaseSinceMillis() >= config::kUpdateRestartDelayMillis) {
    if (beforeRestart_) beforeRestart_();
    LOG_INFO("update", "restarting");
    delay(100);   // let the log line out
    ESP.restart();
  }

  // The rollback test build never counts as healthy, so it is never declared good.
  switch (validator_.update(healthy && !config::kTestUpdateNeverConfirms, nowMillis)) {
    case BootValidator::Action::None:
      break;
    case BootValidator::Action::MarkGood:
      esp_ota_mark_app_valid_cancel_rollback();
      LOG_INFO("update", "firmware %s is good: it stays", config::kFirmwareVersion);
      break;
    case BootValidator::Action::RollBack:
      LOG_WARN("update", "firmware %s didn't stay on Wi-Fi: going back to the previous firmware",
               config::kFirmwareVersion);
      if (beforeRestart_) beforeRestart_();
      delay(100);
      esp_ota_mark_app_invalid_rollback_and_reboot();   // restarts; returns only on failure
      LOG_ERROR("update", "could not go back to the previous firmware");
      break;
  }
}

void FirmwareUpdate::publish(uint32_t nowMillis) {
  (void)nowMillis;
  // Every change of phase passes through here: pause the audio analysis while
  // an update is being received or is about to restart, resume after a failure.
  const bool pause = busy();
  if (pause != othersPaused_) {
    othersPaused_ = pause;
    if (pauseHook_) pauseHook_(pause);
    LOG_INFO("update", "audio analysis %s", pause ? "paused for the update" : "resumed");
  }

  UpdateProgress p;
  p.phase = toProgressPhase(session_.phase());
  p.phaseSinceMillis = session_.phaseSinceMillis();
  if (p.phase == published_.phase && p.phaseSinceMillis == published_.phaseSinceMillis) {
    return;   // unchanged: don't take the lock every loop
  }
  published_ = p;
  progress_.set(p);
}

void FirmwareUpdate::logTransfer(const char* outcome, uint32_t nowMillis) {
  const uint32_t ms = nowMillis - uploadStartMillis_;
  const unsigned kb = static_cast<unsigned>(session_.received() / 1024);
  LOG_INFO("update",
           "transfer %s: %u KB in %.1f s (%.0f KB/s), longest gap between pieces %lu ms, "
           "last piece %lu ms ago, Wi-Fi %d dBm, free heap %lu",
           outcome, kb, ms / 1000.0f, ms > 0 ? kb * 1000.0f / ms : 0.0f, (unsigned long)longestGapMillis_,
           (unsigned long)(nowMillis - lastPieceMillis_), (int)WiFi.RSSI(), (unsigned long)ESP.getFreeHeap());
}
