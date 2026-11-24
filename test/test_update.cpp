// Unit tests for firmware updates: the marker scanner, the upload rules
// (UpdateSession), the trial-firmware decision (BootValidator) and the strip's
// progress display (UpdateBar).

#include <cstring>
#include <string>
#include <vector>

#include "TestFramework.h"
#include "config/Config.h"
#include "modes/UpdateBar.h"
#include "update/BootValidator.h"
#include "update/FirmwareMarker.h"
#include "update/UpdateSession.h"

namespace {

// Records what the session asked the flash writer to do.
class FakeWriter : public FirmwareWriter {
 public:
  bool begin() override {
    ++begins;
    written.clear();
    return beginOk;
  }
  bool write(const uint8_t* data, size_t len) override {
    if (!writeOk) return false;
    written.insert(written.end(), data, data + len);
    return true;
  }
  bool finish() override {
    finished = finishOk;
    return finishOk;
  }
  void abort() override { ++aborts; }
  std::string error() const override { return "fake error"; }

  bool beginOk = true, writeOk = true, finishOk = true;
  int begins = 0, aborts = 0;
  bool finished = false;
  std::vector<uint8_t> written;
};

// A pretend firmware image: magic byte, filler, the marker somewhere, filler.
std::vector<uint8_t> image(size_t size, bool withMarker = true, size_t markerAt = 1000) {
  std::vector<uint8_t> v(size);
  for (size_t i = 0; i < size; ++i) v[i] = static_cast<uint8_t>(i * 7 + 3);
  v[0] = 0xE9;
  if (withMarker) std::memcpy(&v[markerAt], firmware_marker::kText, firmware_marker::kLength);
  return v;
}

// Sends an image in chunks, like the web server does.
void send(UpdateSession& s, const std::vector<uint8_t>& data, size_t chunk, uint32_t& now) {
  for (size_t i = 0; i < data.size(); i += chunk) {
    const size_t n = std::min(chunk, data.size() - i);
    s.write(&data[i], n, now);
    now += 5;
  }
}

constexpr size_t kMax = 100000;

}  // namespace

// ---- Marker ------------------------------------------------------------------------------

TEST(marker_found_across_chunk_boundaries) {
  const auto img = image(5000, true, 2000);
  for (size_t chunk : {1, 2, 7, 17, 1436, 5000}) {   // every split point gets exercised
    MarkerScanner m;
    for (size_t i = 0; i < img.size(); i += chunk) m.feed(&img[i], std::min(chunk, img.size() - i));
    if (!m.found()) std::printf("      not found with chunk %zu\n", chunk);
    CHECK(m.found());
  }
}

TEST(marker_not_found_in_other_data) {
  MarkerScanner m;
  const auto img = image(20000, false);
  m.feed(img.data(), img.size());
  CHECK(!m.found());
  // Nearly the marker (last character different) is not the marker.
  std::string near(firmware_marker::kText);
  near.back() ^= 1;
  m.feed(reinterpret_cast<const uint8_t*>(near.data()), near.size());
  CHECK(!m.found());
}

TEST(marker_found_after_a_false_start) {
  // A partial match that breaks off right before the real marker (needs the KMP fallback).
  std::string s = "xx";
  s += std::string(firmware_marker::kText, 10);   // starts like the marker, then...
  s += firmware_marker::kText;                     // ...the real one begins immediately
  MarkerScanner m;
  m.feed(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  CHECK(m.found());
}

// ---- Upload rules --------------------------------------------------------------------------

TEST(update_good_file_installs) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  const auto img = image(50000);
  CHECK(s.start(img.size() + 300, now) == "");   // multipart adds a little to the length
  send(s, img, 1436, now);
  CHECK(s.phase() == UpdateSession::Phase::Receiving);
  CHECK(s.percent() >= 98 && s.percent() <= 99);   // never 100 before it's checked
  s.finish(now);
  CHECK(s.phase() == UpdateSession::Phase::Succeeded);
  CHECK_EQ(s.percent(), 100);
  CHECK(w.finished);
  CHECK(w.written == img);
  CHECK_EQ(w.aborts, 0);
}

TEST(update_merged_bin_refused_before_writing) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  const std::string err = s.start(8u * 1024 * 1024, 0);   // the IDE's .merged.bin: all 8 MB
  CHECK(err.find(".merged.bin") != std::string::npos);
  CHECK(err.find("SpectrumAnalyzer.ino.esp32s3.bin") != std::string::npos);   // says which file instead
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK_EQ(w.begins, 0);   // the flash was never touched
}

TEST(update_too_big_with_unknown_length) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  CHECK(s.start(0, now) == "");
  send(s, image(kMax + 10), 4096, now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK(s.error().find(".merged.bin") != std::string::npos);
  CHECK_EQ(w.aborts, 1);
  CHECK(w.written.size() <= kMax);
}

TEST(update_not_a_firmware_file) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  auto img = image(5000);
  img[0] = 'P';   // e.g. a PNG or a zip
  s.start(img.size(), now);
  send(s, img, 1000, now);
  s.finish(now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK(s.error().find("isn't a firmware file") != std::string::npos);
  CHECK(w.written.empty());
  CHECK(!w.finished);
}

TEST(update_other_sketch_refused) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  const auto img = image(30000, false);   // a valid-looking image without our marker
  s.start(img.size(), now);
  send(s, img, 1436, now);
  s.finish(now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK(s.error().find("another sketch") != std::string::npos);
  CHECK(!w.finished);   // never made bootable
  CHECK_EQ(w.aborts, 1);
}

TEST(update_writer_rejects_image) {
  // e.g. built for another chip, or corrupted: the Update library's check fails at the end.
  FakeWriter w;
  w.finishOk = false;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  const auto img = image(30000);
  s.start(img.size(), now);
  send(s, img, 1436, now);
  s.finish(now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK(s.error().find("keep their current version") != std::string::npos);
}

TEST(update_write_failure_and_begin_failure) {
  FakeWriter w;
  w.writeOk = false;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  s.start(10000, now);
  send(s, image(10000), 1000, now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK(s.error().find("fake error") != std::string::npos);

  FakeWriter w2;
  w2.beginOk = false;
  UpdateSession s2(w2, kMax, 15000);
  CHECK(!s2.start(10000, 0).empty());
  CHECK(s2.phase() == UpdateSession::Phase::Failed);
}

TEST(update_stalled_upload_gives_up) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  s.start(50000, now);
  send(s, image(20000), 1436, now);   // phone walks out of range part-way
  s.tick(now + 14000);
  CHECK(s.phase() == UpdateSession::Phase::Receiving);
  s.tick(now + 15001);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK_EQ(w.aborts, 1);
  // A fresh attempt afterwards works.
  now += 20000;
  const auto img = image(30000);
  CHECK(s.start(img.size(), now) == "");
  send(s, img, 1436, now);
  s.finish(now);
  CHECK(s.phase() == UpdateSession::Phase::Succeeded);
}

TEST(update_second_upload_refused_while_busy_or_restarting) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  const auto img = image(30000);
  s.start(img.size(), now);
  CHECK(s.start(img.size(), now).find("already") != std::string::npos);
  CHECK(s.phase() == UpdateSession::Phase::Receiving);   // the first one carries on
  send(s, img, 1436, now);
  s.finish(now);
  CHECK(s.start(img.size(), now).find("restarting") != std::string::npos);
  CHECK(s.phase() == UpdateSession::Phase::Succeeded);
  CHECK_EQ(w.begins, 1);
}

TEST(update_cut_off_upload) {
  FakeWriter w;
  UpdateSession s(w, kMax, 15000);
  uint32_t now = 0;
  s.start(50000, now);
  send(s, image(10000), 1436, now);
  s.cancel("The upload was interrupted.", now);
  CHECK(s.phase() == UpdateSession::Phase::Failed);
  CHECK_EQ(w.aborts, 1);
  CHECK(!w.finished);
}

// ---- Trial firmware --------------------------------------------------------------------------

TEST(boot_not_on_trial_does_nothing) {
  BootValidator v(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis);
  v.begin(false, 0);
  for (uint32_t t = 0; t < 10u * 60 * 1000; t += 1000) CHECK(v.update(t > 5000, t) == BootValidator::Action::None);
}

TEST(boot_good_after_30_s_on_wifi) {
  BootValidator v(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis);
  v.begin(true, 0);
  CHECK(v.update(false, 0) == BootValidator::Action::None);
  CHECK(v.update(true, 8000) == BootValidator::Action::None);     // Wi-Fi up after 8 s
  CHECK(v.update(true, 37999) == BootValidator::Action::None);
  CHECK(v.update(true, 38000) == BootValidator::Action::MarkGood);
  CHECK(!v.onTrial());
  CHECK(v.update(true, 39000) == BootValidator::Action::None);    // only once
}

TEST(boot_good_needs_30_s_without_a_break) {
  BootValidator v(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis);
  v.begin(true, 0);
  v.update(true, 1000);
  v.update(true, 25000);
  v.update(false, 26000);   // dropped off Wi-Fi
  CHECK(v.update(true, 27000) == BootValidator::Action::None);
  CHECK(v.update(true, 56999) == BootValidator::Action::None);
  CHECK(v.update(true, 57000) == BootValidator::Action::MarkGood);
}

TEST(boot_rolls_back_after_5_min_without_wifi) {
  BootValidator v(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis);
  v.begin(true, 0);
  BootValidator::Action a = BootValidator::Action::None;
  uint32_t t = 0;
  for (; t <= 6u * 60 * 1000 && a == BootValidator::Action::None; t += 100) a = v.update(false, t);
  CHECK(a == BootValidator::Action::RollBack);
  CHECK(t - 100 == config::kUpdateGiveUpMillis);
  CHECK(v.update(false, t) == BootValidator::Action::None);
}

TEST(boot_flaky_wifi_rolls_back) {
  // On and off every 20 s: never 30 s in a row, so it goes back at 5 min.
  BootValidator v(config::kUpdateGoodAfterMillis, config::kUpdateGiveUpMillis);
  v.begin(true, 0);
  BootValidator::Action a = BootValidator::Action::None;
  uint32_t t = 0;
  for (; a == BootValidator::Action::None; t += 100) a = v.update((t / 20000) % 2 == 1, t);
  CHECK(a == BootValidator::Action::RollBack);
}

// ---- Strip display ---------------------------------------------------------------------------

TEST(update_bar_fills_left_to_right) {
  Rgb out[150];
  UpdateProgress p;
  CHECK(!update_bar::render(p, 0, 2000, out, 150));   // idle: the mode draws as usual
  p.phase = UpdateProgress::Phase::Receiving;
  p.percent = 0;
  CHECK(update_bar::render(p, 0, 2000, out, 150));
  CHECK(out[0] == update_bar::kTrack && out[149] == update_bar::kTrack);
  p.percent = 50;
  update_bar::render(p, 0, 2000, out, 150);
  CHECK(out[0] == update_bar::kBar && out[74] == update_bar::kBar);
  CHECK(out[75] == update_bar::kTrack && out[149] == update_bar::kTrack);
  p.percent = 33;   // 49.5 LEDs: the 50th is half-way between track and bar
  update_bar::render(p, 0, 2000, out, 150);
  CHECK(out[48] == update_bar::kBar);
  CHECK(out[49] != update_bar::kBar && out[49] != update_bar::kTrack);
  CHECK(out[50] == update_bar::kTrack);
  p.percent = 100;
  update_bar::render(p, 0, 2000, out, 150);
  CHECK(out[149] == update_bar::kBar);
}

TEST(update_bar_success_and_failure_flashes) {
  Rgb out[10];
  UpdateProgress p;
  p.phase = UpdateProgress::Phase::Succeeded;
  p.phaseSinceMillis = 1000;
  CHECK(update_bar::render(p, 1000, 2000, out, 10) && out[0] == update_bar::kGood);
  CHECK(update_bar::render(p, 1300, 2000, out, 10) && out[0] == Rgb{});
  CHECK(update_bar::render(p, 60000, 2000, out, 10));   // green until the restart
  p.phase = UpdateProgress::Phase::Failed;
  CHECK(update_bar::render(p, 1100, 2000, out, 10) && out[5] == update_bar::kBad);
  CHECK(!update_bar::render(p, 3000, 2000, out, 10));   // then back to the mode
}
