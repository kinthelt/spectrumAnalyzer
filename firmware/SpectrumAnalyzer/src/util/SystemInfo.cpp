#include "SystemInfo.h"

#include <Arduino.h>
#include <esp_system.h>

#include "../config/Config.h"
#include "Log.h"

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout (supply voltage dipped)";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_EXT: return "reset pin";
    default: return "other";
  }
}

void logBootInfo() {
  LOG_INFO("boot", "spectrumAnalyzer %s", config::kFirmwareVersion);
  LOG_INFO("boot", "reset reason: %s", resetReasonText());
  LOG_INFO("boot", "PSRAM %lu B, flash %lu B", (unsigned long)ESP.getPsramSize(),
           (unsigned long)ESP.getFlashChipSize());
}
