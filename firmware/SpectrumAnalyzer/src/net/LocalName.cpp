#include "LocalName.h"

#include <ESPmDNS.h>
#include <mdns.h>

#include "../config/Config.h"
#include "../util/Log.h"
#include "StationIp.h"

namespace {

mdns_ip_addr_t addressOf(uint32_t ip) {
  mdns_ip_addr_t a = {};
  a.addr.type = ESP_IPADDR_TYPE_V4;
  a.addr.u_addr.ip4.addr = ip;
  a.next = nullptr;
  return a;
}

}  // namespace

void LocalName::update(bool hasIp, uint32_t ip) {
  if (!hasIp) return;   // keep the name registered; it is updated when the address returns

  if (!mdnsStarted_) {
    // Main hostname: deliberately not "spectrum" (see header).
    if (!MDNS.begin("spectrum-device")) {
      LOG_ERROR("net", "mDNS failed to start; use the IP address instead");
      mdnsStarted_ = true;   // don't retry every loop
      return;
    }
    mdnsStarted_ = true;
  }

  if (!active_) {
    const mdns_ip_addr_t address = addressOf(ip);
    const esp_err_t host = mdns_delegate_hostname_add(config::kHostname, &address);
    const esp_err_t service = mdns_service_add_for_host("Spectrum Analyzer", "_http", "_tcp",
                                                        config::kHostname, 80, nullptr, 0);
    if (host == ESP_OK) {
      active_ = true;
      ip_ = ip;
      LOG_INFO("net", "reachable at http://%s.local (%s); web service %s", config::kHostname,
               ipToString(ip).c_str(), esp_err_to_name(service));
    } else {
      LOG_ERROR("net", "could not register %s.local: %s", config::kHostname, esp_err_to_name(host));
      mdnsStarted_ = true;
    }
    return;
  }

  if (ip != ip_) {   // new address from the router
    const mdns_ip_addr_t address = addressOf(ip);
    mdns_delegate_hostname_set_address(config::kHostname, &address);
    ip_ = ip;
    LOG_INFO("net", "%s.local now points to %s", config::kHostname, ipToString(ip).c_str());
  }
}
