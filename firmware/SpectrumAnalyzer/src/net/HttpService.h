// Something that serves HTTP on port 80 (the web page, src/web/WebUi).
// Connectivity starts it when the lights are on the home Wi-Fi and stops it
// while the Wi-Fi setup network is open, because the setup page needs port 80.
#pragma once

class HttpService {
 public:
  virtual ~HttpService() = default;
  virtual void start() = 0;   // both are safe to call repeatedly
  virtual void stop() = 0;
  virtual void loop() = 0;    // handles waiting requests; called from the Arduino loop
};
