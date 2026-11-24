// The rules for receiving a firmware file over Wi-Fi, apart from the flash
// writing itself (FirmwareWriter), so they are unit-tested on a PC.
//
// A file is refused unless it:
//   - fits in the spare firmware slot (the IDE's ".merged.bin" is the whole flash
//     and doesn't: the message says which file to choose instead);
//   - starts like an ESP32 firmware image (first byte 0xE9);
//   - contains this project's marker (FirmwareMarker.h), so another sketch's
//     firmware is refused;
//   - passes the writer's own checks at the end (complete, checksum, built for
//     an ESP32-S3).
// Only then does it become the firmware the board starts next. A refused or
// interrupted upload leaves the running firmware as it is.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "FirmwareMarker.h"

// Writes a firmware image into the spare slot. Implemented with the Update
// library on the board (FirmwareUpdate.cpp) and faked in the tests.
class FirmwareWriter {
 public:
  virtual ~FirmwareWriter() = default;
  virtual bool begin() = 0;                                  // start writing the spare slot
  virtual bool write(const uint8_t* data, size_t len) = 0;   // false if not all written
  virtual bool finish() = 0;   // verify the image, make it the one started next
  virtual void abort() = 0;    // throw away what was written
  virtual std::string error() const = 0;
};

class UpdateSession {
 public:
  enum class Phase : uint8_t { Idle, Receiving, Succeeded, Failed };

  UpdateSession(FirmwareWriter& writer, size_t maxBytes, uint32_t stallMillis)
      : writer_(writer), maxBytes_(maxBytes), stallMillis_(stallMillis) {}

  // A new upload of about expectedBytes (0 = unknown; used for the percentage).
  // Returns "" if it started, or why not. A refusal because an update is already
  // running or has just finished leaves that update alone; any other refusal
  // marks this one Failed.
  std::string start(size_t expectedBytes, uint32_t nowMillis);
  void write(const uint8_t* data, size_t len, uint32_t nowMillis);
  // The whole file has arrived: checks it and, if good, makes it the next firmware.
  void finish(uint32_t nowMillis);
  // The upload was cut off (connection lost).
  void cancel(const char* why, uint32_t nowMillis);
  // Gives up on an upload that has stopped sending data. Call every loop.
  void tick(uint32_t nowMillis);

  Phase phase() const { return phase_; }
  uint32_t phaseSinceMillis() const { return phaseSince_; }
  uint8_t percent() const;
  size_t received() const { return received_; }
  const std::string& error() const { return error_; }   // why the last update failed
  size_t maxBytes() const { return maxBytes_; }
  void setMaxBytes(size_t maxBytes) { maxBytes_ = maxBytes; }   // the spare slot's size

 private:
  void fail(const std::string& why, uint32_t nowMillis);
  void setPhase(Phase p, uint32_t nowMillis) {
    phase_ = p;
    phaseSince_ = nowMillis;
  }

  FirmwareWriter& writer_;
  size_t maxBytes_;
  const uint32_t stallMillis_;
  Phase phase_ = Phase::Idle;
  uint32_t phaseSince_ = 0;
  uint32_t lastDataMillis_ = 0;
  size_t expected_ = 0;
  size_t received_ = 0;
  MarkerScanner marker_;
  std::string error_;
};
