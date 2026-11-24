#include "StatusPage.h"

void StatusPage::start() {
  if (running_) return;
  if (!routesAdded_) {
    server_.on("/", [this]() {
      server_.send(200, "text/plain; charset=utf-8",
                   describe_ ? describe_() : String("Spectrum Analyzer\n"));
    });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found\n"); });
    routesAdded_ = true;
  }
  server_.begin();
  running_ = true;
}

void StatusPage::stop() {
  if (!running_) return;
  server_.stop();
  running_ = false;
}

void StatusPage::loop() {
  if (running_) server_.handleClient();
}
