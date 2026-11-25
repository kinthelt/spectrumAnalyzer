#include "WebUi.h"

#include <map>
#include <string>

#include "../util/Log.h"
#include "WebPage.h"

namespace {

// How long Forget Wi-Fi waits after replying, so the phone gets the answer
// before the lights leave the network.
constexpr uint32_t kForgetDelayMillis = 500;

// The request's form fields, copied once (WebServer::arg returns temporaries).
class ServerArgs : public FormArgs {
 public:
  explicit ServerArgs(WebServer& server) {
    for (int i = 0; i < server.args(); ++i) {
      if (server.argName(i) == "plain") continue;   // the raw body, not a field
      values_[server.argName(i).c_str()] = server.arg(i).c_str();
    }
  }
  const char* get(const char* name) const override {
    auto it = values_.find(name);
    return it == values_.end() ? nullptr : it->second.c_str();
  }

 private:
  std::map<std::string, std::string> values_;
};

}  // namespace

void WebUi::start() {
  if (running_) return;
  if (!routesAdded_) {
    addRoutes();
    routesAdded_ = true;
  }
  server_.begin();
  running_ = true;
  LOG_INFO("web", "web page running on port 80");
}

void WebUi::stop() {
  if (!running_) return;
  server_.stop();
  running_ = false;
}

void WebUi::loop() {
  if (running_) server_.handleClient();
  if (forgetPending_ && millis() - forgetRequestedMillis_ >= kForgetDelayMillis) {
    forgetPending_ = false;
    if (hooks_.forgetWifi) hooks_.forgetWifi();
  }
}

void WebUi::addRoutes() {
  static const char* headers[] = {"If-None-Match"};
  server_.collectHeaders(headers, 1);

  server_.on("/", HTTP_GET, [this]() { sendPage(); });
  server_.on("/index.html", HTTP_GET, [this]() { sendPage(); });
  server_.on("/api/state", HTTP_GET, [this]() { sendState(); });
  server_.on("/api/state", HTTP_POST, [this]() { handleState(); });
  for (const char* action : {"save", "recall", "rename", "delete"}) {
    const String path = String("/api/preset/") + action;
    server_.on(path.c_str(), HTTP_POST, [this, action]() { handlePreset(action); });
  }
  for (const char* action : {"learn-noise", "forget-wifi", "reset-settings", "home-pair", "home-unpair"}) {
    const String path = String("/api/action/") + action;
    server_.on(path.c_str(), HTTP_POST, [this, action]() { handleAction(action); });
  }
  server_.on("/api/update", HTTP_POST, [this]() { handleUploadDone(); },
             [this]() { handleUploadChunk(); });
  server_.onNotFound([this]() { sendError(404, "not found"); });
}

void WebUi::sendPage() {
  // The ETag changes with the page, so phones re-download only after an update.
  server_.sendHeader("Cache-Control", "no-cache");
  server_.sendHeader("ETag", web_page::kEtag);
  if (server_.header("If-None-Match") == web_page::kEtag) {
    server_.send(304);
    return;
  }
  server_.sendHeader("Content-Encoding", "gzip");
  server_.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(web_page::kGzip),
                 web_page::kGzipSize);
}

void WebUi::sendState(int code) {
  const DeviceStatus status = hooks_.status ? hooks_.status() : DeviceStatus{};
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", web_api::stateJson(settings_, status).c_str());
}

void WebUi::sendError(int code, const std::string& message) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", web_api::errorJson(message).c_str());
}

void WebUi::handleState() {
  const std::string err = web_api::applyState(ServerArgs(server_), settings_, millis());
  if (!err.empty()) return sendError(400, err);
  sendState();
}

void WebUi::handlePreset(const char* action) {
  const std::string err = web_api::presetAction(action, ServerArgs(server_), settings_, millis());
  if (!err.empty()) return sendError(400, err);
  LOG_INFO("web", "preset %s, slot %s", action, server_.arg("slot").c_str());
  sendState();
}

void WebUi::handleAction(const char* action) {
  const String name(action);
  if (name == "learn-noise") {
    LOG_INFO("web", "learn room noise requested");
    if (hooks_.learnNoise) hooks_.learnNoise();
  } else if (name == "reset-settings") {
    LOG_INFO("web", "settings reset requested");
    settings_.factoryReset();
  } else if (name == "forget-wifi") {
    LOG_INFO("web", "forget Wi-Fi requested");
    forgetPending_ = true;   // after this reply has gone out (see loop)
    forgetRequestedMillis_ = millis();
  } else if (name == "home-pair") {
    LOG_INFO("web", "allow pairing requested");
    if (!hooks_.openPairing || !hooks_.openPairing()) {
      return sendError(503, "Pairing couldn't be opened. Are the lights on Wi-Fi?");
    }
  } else if (name == "home-unpair") {
    LOG_INFO("web", "remove from all smart homes requested");
    if (!hooks_.removeAllHomes || !hooks_.removeAllHomes()) {
      return sendError(503, "The pairings couldn't all be removed. Try again in a moment.");
    }
  }
  sendState();
}

// The web server calls this for each piece of the uploaded file. The whole
// upload happens inside one handleClient() call, so the Arduino loop is paused
// meanwhile; the LED task (its own task) keeps the strip in the update colours.
void WebUi::handleUploadChunk() {
  HTTPUpload& up = server_.upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      uploadSeen_ = true;
      uploadRefusedBusy_ = update_.busy();
      // The request's length is the file plus a few hundred bytes of form wrapping.
      uploadRefusal_ = update_.uploadStart(static_cast<size_t>(server_.clientContentLength()), millis());
      break;
    case UPLOAD_FILE_WRITE:
      if (!uploadRefusedBusy_) update_.uploadData(up.buf, up.currentSize, millis());
      break;
    case UPLOAD_FILE_END:
      if (!uploadRefusedBusy_) update_.uploadEnd(millis());
      break;
    case UPLOAD_FILE_ABORTED:
      if (!uploadRefusedBusy_) update_.uploadAborted(millis());
      uploadSeen_ = false;
      uploadRefusedBusy_ = false;
      break;
  }
}

void WebUi::handleUploadDone() {
  const bool seen = uploadSeen_;
  const bool busy = uploadRefusedBusy_;
  uploadSeen_ = false;
  uploadRefusedBusy_ = false;

  if (!seen) return sendError(400, "No file was sent.");
  if (busy) return sendError(409, uploadRefusal_);       // another update is running or done
  if (!update_.succeeded()) return sendError(400, update_.error());   // refused or failed
  sendState();   // the board restarts in a moment (FirmwareUpdate::loop)
}
