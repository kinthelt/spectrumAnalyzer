// A plain-text status page at http://spectrum.local, until the real web page
// arrives in build step 7. It stops whenever the Wi-Fi setup network is open,
// because the setup page needs port 80.
#pragma once

#include <WebServer.h>

#include <functional>

class StatusPage {
 public:
  using Describer = std::function<String()>;

  // describe() returns the page text; called on every request.
  void setDescriber(Describer describe) { describe_ = describe; }

  void start();
  void stop();
  void loop();
  bool running() const { return running_; }

 private:
  WebServer server_{80};
  Describer describe_;
  bool routesAdded_ = false;
  bool running_ = false;
};
