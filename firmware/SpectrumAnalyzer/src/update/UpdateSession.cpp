#include "UpdateSession.h"

namespace {
constexpr uint8_t kImageMagic = 0xE9;   // first byte of every ESP32 firmware image
// Arduino IDE 1.8: Sketch > Export Compiled Binary saves SpectrumAnalyzer.ino.esp32s3.bin
// in the sketch folder (and copies more files, .merged.bin included, into build/).
constexpr const char* kWhichFile =
    "Choose SpectrumAnalyzer.ino.esp32s3.bin from the sketch folder (Sketch > Export Compiled "
    "Binary in the Arduino IDE).";
const std::string kTooBig =
    std::string("That file is too big to be a firmware update (the .merged.bin is the whole flash). ") +
    kWhichFile;
}

std::string UpdateSession::start(size_t expectedBytes, uint32_t nowMillis) {
  if (phase_ == Phase::Receiving) return "An update is already being installed.";
  if (phase_ == Phase::Succeeded) return "An update has just been installed; the lights are restarting.";

  expected_ = expectedBytes;
  received_ = 0;
  lastDataMillis_ = nowMillis;
  marker_.reset();
  error_.clear();
  setPhase(Phase::Receiving, nowMillis);

  if (expectedBytes > maxBytes_) {
    fail(kTooBig,
         nowMillis);
    return error_;
  }
  if (!writer_.begin()) {
    fail("Couldn't start the update (" + writer_.error() + ").", nowMillis);
    return error_;
  }
  return "";
}

void UpdateSession::write(const uint8_t* data, size_t len, uint32_t nowMillis) {
  if (phase_ != Phase::Receiving || len == 0) return;
  if (received_ == 0 && data[0] != kImageMagic) {
    writer_.abort();
    fail(std::string("That isn't a firmware file. ") + kWhichFile, nowMillis);
    return;
  }
  if (received_ + len > maxBytes_) {
    writer_.abort();
    fail(kTooBig,
         nowMillis);
    return;
  }
  marker_.feed(data, len);
  if (!writer_.write(data, len)) {
    writer_.abort();
    fail("Writing the update failed (" + writer_.error() + ").", nowMillis);
    return;
  }
  received_ += len;
  lastDataMillis_ = nowMillis;
}

void UpdateSession::finish(uint32_t nowMillis) {
  if (phase_ != Phase::Receiving) return;
  if (received_ == 0) {
    writer_.abort();
    fail("The file was empty.", nowMillis);
    return;
  }
  if (!marker_.found()) {
    writer_.abort();
    fail(std::string("That isn't firmware for these lights: it's from another sketch, or it's another "
                     "of the exported files. ") + kWhichFile,
         nowMillis);
    return;
  }
  if (!writer_.finish()) {
    writer_.abort();
    fail("The lights refused this firmware (" + writer_.error() + "). They keep their current version.",
         nowMillis);
    return;
  }
  setPhase(Phase::Succeeded, nowMillis);
}

void UpdateSession::cancel(const char* why, uint32_t nowMillis) {
  if (phase_ != Phase::Receiving) return;
  writer_.abort();
  fail(why, nowMillis);
}

void UpdateSession::tick(uint32_t nowMillis) {
  if (phase_ == Phase::Receiving && nowMillis - lastDataMillis_ > stallMillis_) {
    cancel("The upload stopped part-way. The lights keep their current version.", nowMillis);
  }
}

uint8_t UpdateSession::percent() const {
  if (phase_ == Phase::Succeeded) return 100;
  if (expected_ == 0) return 0;
  const size_t p = received_ * 100 / expected_;
  return static_cast<uint8_t>(p > 99 ? 99 : p);   // 100 only once checked and installed
}

void UpdateSession::fail(const std::string& why, uint32_t nowMillis) {
  error_ = why;
  setPhase(Phase::Failed, nowMillis);
}
