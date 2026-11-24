// Throwaway compatibility test for build step 6. Not part of the firmware.
//
// Questions it answers on the real hardware:
//   1. Does Matter pair with Apple Home (and Google Home) on this board?
//   2. Can a Wi-Fi setup portal (WiFiManager) and Matter share the Wi-Fi radio?
//   3. Do Arduino's mDNS (spectrum.local) and Matter's own mDNS coexist?
//   4. Does a paired device reconnect after switching off at the plug?
//   5. With Wi-Fi and Matter running, is the audio task on core 0 still on time?
//
// Pick the experiment below, then in Tools set:
//   Partition Scheme: "Matter 8MB with spiffs"
//   Erase All Flash Before Sketch Upload: Enabled (first upload of each experiment)
// Experiment 2 needs the WiFiManager library by tzapu (Library Manager).
//
// Serial Monitor (115200, Newline): status | decommission | forget | reboot

// 1 = Matter only: Apple/Google Home sends the Wi-Fi details over Bluetooth while pairing.
// 2 = Setup portal first ("Spectrum-Setup" network), then Matter pairs over Wi-Fi.
#define EXPERIMENT 1

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Matter.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include <atomic>
#if EXPERIMENT == 2
#include <WiFiManager.h>
#endif

static MatterColorLight light;
static WebServer web(80);
#if EXPERIMENT == 2
static WiFiManager wifiManager;   // only in experiment 2: nothing else touches Wi-Fi in 1
#endif
static bool netServicesStarted = false;

// ---- Stand-in for the audio task --------------------------------------------
// Every 11 ms it busy-works 4.6 ms on core 0 at the audio task's priority, like
// the real filter bank (43% of the core), and records the longest gap between
// its wake-ups. The real audio task has ~40 ms of microphone buffering, so gaps
// under ~30 ms are harmless.
static std::atomic<uint32_t> probeRuns{0};
static std::atomic<uint32_t> probeMaxGapUs{0};
static std::atomic<uint32_t> probeLongGaps{0};   // gaps over 30 ms

static void loadProbe(void*) {
  const TickType_t period = pdMS_TO_TICKS(11);
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastStart = micros();
  for (;;) {
    vTaskDelayUntil(&lastWake, period);
    const uint32_t now = micros();
    const uint32_t gap = now - lastStart;
    lastStart = now;
    if (probeRuns > 0) {
      if (gap > probeMaxGapUs) probeMaxGapUs = gap;
      if (gap > 30000) ++probeLongGaps;
    }
    while (micros() - now < 4600) {
    }
    ++probeRuns;
  }
}

// ---- Network services: mDNS name and a test page --------------------------------
static void startNetServices() {
  if (MDNS.begin("spectrum")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: started as spectrum.local");
  } else {
    Serial.println("mDNS: FAILED to start");
  }
  web.on("/", []() {
    String page = "Spectrum spike OK\nIP " + WiFi.localIP().toString() +
                  "\nMatter commissioned: " + (Matter.isDeviceCommissioned() ? "yes" : "no") +
                  "\nLight: " + (light.getOnOff() ? "on" : "off") + "\n";
    web.send(200, "text/plain", page);
  });
  web.begin();
  Serial.printf("Web: open http://spectrum.local or http://%s\n", WiFi.localIP().toString().c_str());
  netServicesStarted = true;
}

static void printPairingInfo() {
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter: already paired");
    return;
  }
  Serial.printf("Matter: not paired. Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
  Serial.printf("Matter: QR code page: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
}

static void printStatus() {
  const bool wifiUp = WiFi.status() == WL_CONNECTED;
  Serial.printf("wifi %s %s rssi %d | matter paired %d online %d | heap %lu | ",
                wifiUp ? "UP" : "down", wifiUp ? WiFi.localIP().toString().c_str() : "-",
                wifiUp ? WiFi.RSSI() : 0, Matter.isDeviceCommissioned(), Matter.isOnline(),
                (unsigned long)ESP.getFreeHeap());
  Serial.printf("core-0 probe: %lu runs, longest gap %lu us, gaps >30 ms: %lu\n",
                (unsigned long)probeRuns.load(), (unsigned long)probeMaxGapUs.load(),
                (unsigned long)probeLongGaps.load());
  probeMaxGapUs = 0;
}

static void handleSerial() {
  static String line;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      line += c;
      continue;
    }
    line.trim();
    if (line == "status") {
      printStatus();
      printPairingInfo();
    } else if (line == "decommission") {
      Serial.println("Removing Matter pairing; restarting");
      Matter.decommission();
    } else if (line == "forget") {
      Serial.println("Forgetting Wi-Fi; restarting");
#if EXPERIMENT == 2
      wifiManager.resetSettings();
#endif
      WiFi.disconnect(true, true);
      delay(200);
      ESP.restart();
    } else if (line == "reboot") {
      ESP.restart();
    } else if (line.length() > 0) {
      Serial.println("Commands: status | decommission | forget | reboot");
    }
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Wi-Fi/Matter spike, experiment %d ===\n", EXPERIMENT);
  Serial.printf("Reset reason %d, heap %lu, PSRAM %lu\n", (int)esp_reset_reason(),
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getPsramSize());

  xTaskCreatePinnedToCore(loadProbe, "probe", 4096, nullptr, 5, nullptr, 0);

#if EXPERIMENT == 2
  wifiManager.setHostname("spectrum");
  wifiManager.setConfigPortalTimeout(300);
  Serial.println("Wi-Fi: connecting; if not set up, join the 'Spectrum-Setup' network");
  if (!wifiManager.autoConnect("Spectrum-Setup")) {
    Serial.println("Wi-Fi: setup timed out, restarting");
    ESP.restart();
  }
  Serial.printf("Wi-Fi: connected via portal, IP %s\n", WiFi.localIP().toString().c_str());
  Matter.setBLECommissioningEnabled(false);   // pair over the Wi-Fi network instead
#endif

  Matter.setDeviceName("Spectrum Analyzer");
  light.begin(false, {0, 254, 64});
  light.onChange([](bool on, espHsvColor_t hsv) {
    Serial.printf("Matter -> light %s, hue %u sat %u value %u\n", on ? "ON" : "OFF", hsv.h, hsv.s,
                  hsv.v);
    return true;
  });
  Matter.begin();
  Serial.printf("Matter: started, BLE pairing %s\n",
                Matter.isBLECommissioningEnabled() ? "enabled" : "disabled");
  printPairingInfo();
}

void loop() {
  static uint32_t lastStatus = 0;
  handleSerial();

  if (!netServicesStarted && WiFi.status() == WL_CONNECTED) startNetServices();
  if (netServicesStarted) web.handleClient();

  if (millis() - lastStatus >= 5000) {
    lastStatus = millis();
    printStatus();
  }
  delay(2);
}
