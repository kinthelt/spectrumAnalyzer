// Makes the board reachable as http://spectrum.local.
//
// "spectrum" is registered as a *delegated* mDNS host, not as the main hostname:
// Matter (step 9) renames the main hostname to its own hex name, which made
// spectrum.local stop working in the Wi-Fi/Matter spike, while a delegated name
// kept working. The main hostname is set to something else so the two never clash.
#pragma once

#include <cstdint>

class LocalName {
 public:
  // Call every loop with the station's current address (or hasIp = false).
  void update(bool hasIp, uint32_t ip);

  bool active() const { return active_; }

 private:
  bool mdnsStarted_ = false;
  bool active_ = false;
  uint32_t ip_ = 0;
};
