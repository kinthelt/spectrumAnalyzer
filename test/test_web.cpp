// Unit tests for src/util/JsonWriter and src/web/WebApi.
// JSON produced here is also written to build/json/ and checked by Python's
// strict parser after the tests run (see Makefile).

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

#include "FakeSettingsStore.h"
#include "TestFramework.h"
#include "util/JsonWriter.h"
#include "web/WebApi.h"

namespace {

class FakeArgs : public FormArgs {
 public:
  FakeArgs(std::initializer_list<std::pair<const std::string, std::string>> v) : values_(v) {}
  const char* get(const char* name) const override {
    auto it = values_.find(name);
    return it == values_.end() ? nullptr : it->second.c_str();
  }

 private:
  std::map<std::string, std::string> values_;
};

void dumpJson(const char* name, const std::string& json) {
  std::ofstream(std::string("build/json/") + name + ".json") << json;
}

DeviceStatus sampleStatus() {
  DeviceStatus st;
  st.version = "0.7.0";
  st.wifiConnected = true;
  st.ssid = "Home \"5G\" Wi-Fi";   // quotes in an SSID must be escaped
  st.rssi = -58;
  st.ip = "192.168.3.234";
  st.hostname = "spectrum";
  st.audioOk = true;
  st.silent = false;
  st.audioLoadPercent = 43;
  st.uptimeSeconds = 1234;
  st.updateMaxBytes = 3342336;
  return st;
}

}  // namespace

// ---- JsonWriter ---------------------------------------------------------------------------

TEST(json_writer_structure_and_commas) {
  JsonWriter j;
  j.beginObject();
  j.key("a").value(1);
  j.key("b").beginArray().value(true).value("x").beginObject().endObject().endArray();
  j.key("c").beginObject().key("d").value(static_cast<uint32_t>(4000000000u)).endObject();
  j.endObject();
  CHECK(j.str() == R"({"a":1,"b":[true,"x",{}],"c":{"d":4000000000}})");
  dumpJson("writer_structure", j.str());
}

TEST(json_writer_escapes_strings) {
  JsonWriter j;
  j.beginObject().key("s").value("quote\" back\\ nl\n tab\t ctl\x01 café 🎵").endObject();
  CHECK(j.str() == "{\"s\":\"quote\\\" back\\\\ nl\\n tab\\t ctl\\u0001 café 🎵\"}");
  dumpJson("writer_escapes", j.str());
}

// ---- State JSON -----------------------------------------------------------------------------

TEST(web_state_json_has_everything_the_page_needs) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  settings.savePreset(0, "Movie \"night\" 🎬");
  const std::string json = web_api::stateJson(settings, sampleStatus());
  dumpJson("state", json);
  for (const char* field : {"\"revision\":", "\"on\":true", "\"mode\":\"spectrum-smooth\"",
                            "\"brightness\":64", "\"colourA\":\"#FFA03C\"", "\"spectrumColour\":\"rainbow\"",
                            "\"modes\":[\"spectrum-snappy\"", "\"presets\":[",
                            "\"name\":\"Movie \\\"night\\\" 🎬\"", "\"used\":true",
                            "\"name\":\"Preset 2\",\"used\":false", "\"url\":\"http://192.168.3.234/\"",
                            "\"localUrl\":\"http://spectrum.local/\"", "\"ssid\":\"Home \\\"5G\\\" Wi-Fi\"",
                            "\"loadPercent\":43",
                            "\"update\":{\"maxBytes\":3342336,\"onTrial\":false,\"lastUndone\":false}"}) {
    if (json.find(field) == std::string::npos) {
      CHECK(json.find(field) != std::string::npos);
      std::printf("      missing: %s\n", field);
    }
  }
}

TEST(web_state_json_smart_home_codes_only_while_pairing_is_open) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  DeviceStatus st = sampleStatus();
  st.homeStarted = true;
  st.homeApps = {"Apple Home", "Google Home"};
  st.homeManualCode = "34970112332";
  st.homeQrPayload = "MT:Y.K9042C00KA0648G00";
  std::string json = web_api::stateJson(settings, st);
  CHECK(json.find("\"homes\":[\"Apple Home\",\"Google Home\"]") != std::string::npos);
  CHECK(json.find("34970112332") == std::string::npos);   // closed: no codes
  CHECK(json.find("MT:") == std::string::npos);
  st.homePairingOpen = true;
  json = web_api::stateJson(settings, st);
  dumpJson("state_pairing", json);
  CHECK(json.find("\"code\":\"34970112332\"") != std::string::npos);
  CHECK(json.find("\"qr\":\"MT:Y.K9042C00KA0648G00\"") != std::string::npos);
}

TEST(web_state_json_when_offline) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  DeviceStatus st;
  st.version = "0.7.0";
  st.hostname = "spectrum";
  const std::string json = web_api::stateJson(settings, st);
  dumpJson("state_offline", json);
  CHECK(json.find("\"url\":\"\"") != std::string::npos);
  CHECK(json.find("\"connected\":false") != std::string::npos);
}

// ---- Changing state ---------------------------------------------------------------------------

TEST(web_apply_state_changes_fields) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  CHECK(web_api::applyState(FakeArgs{{"brightness", "200"}, {"mode", "solid"},
                                     {"colourA", "#00ff80"}, {"on", "0"}},
                            settings, 0) == "");
  const LightSettings& s = settings.current();
  CHECK_EQ(s.brightness, 200);
  CHECK(s.mode == Mode::Solid);
  CHECK(s.colourA == (Rgb{0, 255, 128}));
  CHECK(!s.on);
  CHECK(web_api::applyState(FakeArgs{{"spectrumColour", "fixed"}, {"colourB", "102030"}}, settings,
                            0) == "");
  CHECK(settings.current().spectrumColour == SpectrumColour::Fixed);
  CHECK(settings.current().colourB == (Rgb{0x10, 0x20, 0x30}));
}

TEST(web_apply_state_is_all_or_nothing) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  const LightSettings before = settings.current();
  const std::pair<std::string, std::string> bad[] = {
      {"brightness", "256"}, {"brightness", "-1"}, {"brightness", "12a"}, {"brightness", ""},
      {"mode", "disco"},     {"colourA", "#12345"}, {"colourB", "red"},   {"on", "yes"},
      {"spectrumColour", "plaid"}};
  for (const auto& kv : bad) {
    // A valid field (a different one) alongside the bad one must not be applied either.
    const std::pair<const std::string, std::string> valid =
        kv.first == "on" ? std::make_pair(std::string("mode"), std::string("solid"))
                         : std::make_pair(std::string("on"), std::string("0"));
    const std::string err = web_api::applyState(FakeArgs{valid, kv}, settings, 0);
    if (err.empty()) std::printf("      accepted bad %s=%s\n", kv.first.c_str(), kv.second.c_str());
    CHECK(!err.empty());
    CHECK(settings.current() == before);
  }
  const std::string err = web_api::applyState(FakeArgs{{"mode", "disco"}}, settings, 0);
  CHECK(err.find("disco") != std::string::npos);   // says what was wrong
  dumpJson("error", web_api::errorJson(err));
}

TEST(web_live_dragging_saves_once) {
  // A slider dragged for 2 s sends ~20 requests; flash is written once afterwards.
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  for (uint32_t t = 0; t < 2000; t += 100) {
    web_api::applyState(FakeArgs{{"brightness", std::to_string(t / 10)}}, settings, t);
    settings.tick(t);
  }
  settings.tick(2000 + SettingsManager::kSaveDelayMillis);
  CHECK_EQ(store.writes, 1);
}

// ---- Presets ------------------------------------------------------------------------------------

TEST(web_preset_actions) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  web_api::applyState(FakeArgs{{"mode", "twinkle"}}, settings, 0);
  CHECK(web_api::presetAction("save", FakeArgs{{"slot", "3"}, {"name", "Stars"}}, settings, 0) == "");
  CHECK(std::string(settings.presetName(2)) == "Stars");
  CHECK(web_api::presetAction("rename", FakeArgs{{"slot", "3"}, {"name", "Starry"}}, settings, 0) == "");
  CHECK(std::string(settings.presetName(2)) == "Starry");
  web_api::applyState(FakeArgs{{"mode", "solid"}}, settings, 0);
  CHECK(web_api::presetAction("recall", FakeArgs{{"slot", "3"}}, settings, 0) == "");
  CHECK(settings.current().mode == Mode::Twinkle);
  CHECK(web_api::presetAction("delete", FakeArgs{{"slot", "3"}}, settings, 0) == "");
  CHECK(!settings.presetUsed(2));
}

TEST(web_preset_errors) {
  FakeSettingsStore store;
  SettingsManager settings(store);
  settings.begin();
  CHECK(!web_api::presetAction("save", FakeArgs{{"slot", "0"}}, settings, 0).empty());
  CHECK(!web_api::presetAction("save", FakeArgs{{"slot", "9"}}, settings, 0).empty());
  CHECK(!web_api::presetAction("save", FakeArgs{}, settings, 0).empty());
  CHECK(!web_api::presetAction("recall", FakeArgs{{"slot", "1"}}, settings, 0).empty());   // empty
  CHECK(!web_api::presetAction("rename", FakeArgs{{"slot", "1"}}, settings, 0).empty());   // no name
  CHECK(!web_api::presetAction("explode", FakeArgs{{"slot", "1"}}, settings, 0).empty());
}
