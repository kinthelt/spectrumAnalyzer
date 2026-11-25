#include "SystemInfo.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
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

bool resetWasPowerUp() {
  const esp_reset_reason_t r = esp_reset_reason();
  return r == ESP_RST_POWERON || r == ESP_RST_BROWNOUT;
}

bool resetWasCrash() {
  const esp_reset_reason_t r = esp_reset_reason();
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

void logMemory(const char* stage) {
  constexpr uint32_t kInternal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  LOG_INFO("memory", "%s: internal RAM %u KB free, largest block %u KB, lowest %u KB", stage,
           (unsigned)(heap_caps_get_free_size(kInternal) / 1024),
           (unsigned)(heap_caps_get_largest_free_block(kInternal) / 1024),
           (unsigned)(heap_caps_get_minimum_free_size(kInternal) / 1024));
}

void logBootInfo() {
  LOG_INFO("boot", "spectrumAnalyzer %s", config::kFirmwareVersion);
  LOG_INFO("boot", "reset reason: %s", resetReasonText());
  LOG_INFO("boot", "PSRAM %lu B, flash %lu B", (unsigned long)ESP.getPsramSize(),
           (unsigned long)ESP.getFlashChipSize());
}
