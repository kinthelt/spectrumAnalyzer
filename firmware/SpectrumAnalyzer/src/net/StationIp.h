// The Wi-Fi station's IPv4 address, read straight from ESP-IDF's network layer.
//
// Arduino's WiFi.status() only knows about connections Arduino made itself;
// Matter (step 9) starts Wi-Fi through ESP-IDF directly, so this asks the
// network interface instead. Found on the bench in the Wi-Fi/Matter spike.
#pragma once

#include <Arduino.h>
#include <esp_netif.h>

// Returns true and the address (network byte order) if the station has one.
inline bool stationIp(uint32_t* out) {
  esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta == nullptr) return false;
  esp_netif_ip_info_t info;
  if (esp_netif_get_ip_info(sta, &info) != ESP_OK || info.ip.addr == 0) return false;
  *out = info.ip.addr;
  return true;
}

inline String ipToString(uint32_t ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (unsigned)(ip & 0xff), (unsigned)((ip >> 8) & 0xff),
           (unsigned)((ip >> 16) & 0xff), (unsigned)((ip >> 24) & 0xff));
  return String(buf);
}
