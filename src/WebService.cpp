#include "WebService.h"

#include <LittleFS.h>
#include <WebServer.h>

#include "RssRuntime.h"

namespace {
const char* kUiPath = "/web/index.html";
}

WebService::WebService(SettingsStore& store, WifiService& wifiService)
    : _server(nullptr),
      _store(store),
      _wifiService(wifiService),
      _rssRuntime(nullptr),
      _onSettingsChanged(nullptr),
      _onWifiConnectRequested(nullptr),
      _onFactoryResetRequested(nullptr) {}

WebService::~WebService() { stop(); }

void WebService::begin(uint16_t port) {
  if (_server != nullptr) {
    return;
  }

  _server = new WebServer(port);
  if (_server == nullptr) {
    return;
  }

  registerRoutes();
  _server->begin();
}

void WebService::stop() {
  if (_server == nullptr) {
    return;
  }
  _server->stop();
  delete _server;
  _server = nullptr;
}

void WebService::tick() {
  if (_server != nullptr) {
    _server->handleClient();
  }
}

bool WebService::isRunning() const { return _server != nullptr; }

void WebService::setOnSettingsChanged(SettingsChangedCallback cb) {
  _onSettingsChanged = cb;
}

void WebService::setOnWifiConnectRequested(VoidCallback cb) {
  _onWifiConnectRequested = cb;
}

void WebService::setOnFactoryResetRequested(VoidCallback cb) {
  _onFactoryResetRequested = cb;
}

void WebService::setRssRuntime(RssRuntime* rssRuntime) { _rssRuntime = rssRuntime; }

void WebService::registerRoutes() {
  _server->on("/", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/favicon.ico", HTTP_GET, [this]() { _server->send(204, "text/plain", ""); });
  _server->on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
  _server->on("/ncsi.txt", HTTP_GET, [this]() { _server->send(200, "text/plain", "Microsoft NCSI"); });
  _server->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server->on("/api/messages", HTTP_POST, [this]() { handleMessages(); });
  _server->on("/api/text", HTTP_POST, [this]() { handleText(); });
  _server->on("/api/color", HTTP_POST, [this]() { handleColor(); });
  _server->on("/api/speed", HTTP_POST, [this]() { handleSpeed(); });
  _server->on("/api/brightness", HTTP_POST, [this]() { handleBrightness(); });
  _server->on("/api/appearance", HTTP_POST, [this]() { handleAppearance(); });
  _server->on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });
  _server->on("/api/advanced", HTTP_POST, [this]() { handleAdvanced(); });
  _server->on("/api/rss", HTTP_POST, [this]() { handleRss(); });
  _server->on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
  _server->onNotFound([this]() { handleNotFound(); });
}

String WebService::readBody() const { return _server->arg("plain"); }

bool WebService::parseBodyJson(JsonDocument& doc) const {
  String body = readBody();
  if (body.length() == 0) {
    return false;
  }
  DeserializationError err = deserializeJson(doc, body);
  return !err;
}

void WebService::sendJson(const JsonDocument& doc, int code) const {
  String out;
  serializeJson(doc, out);
  _server->send(code, "application/json", out);
}

void WebService::sendStatusMessage(const char* status) const {
  DynamicJsonDocument doc(128);
  doc["status"] = status;
  sendJson(doc, 200);
}

void WebService::sendError(const char* message, int code) const {
  DynamicJsonDocument doc(256);
  doc["error"] = message;
  sendJson(doc, code);
}

void WebService::handleRoot() const {
  File file = LittleFS.open(kUiPath, "r");
  if (!file) {
    _server->send(404, "text/plain", "UI file not found");
    return;
  }
  _server->streamFile(file, "text/html");
  file.close();
}

void WebService::handleStatus() const {
  const AppSettings& s = _store.settings();
  DynamicJsonDocument doc(6144);

  JsonArray msgs = doc.createNestedArray("messages");
  for (size_t i = 0; i < APP_MAX_MESSAGES; i++) {
    JsonObject m = msgs.createNestedObject();
    m["text"] = s.messages[i].text;
    m["r"] = s.messages[i].r;
    m["g"] = s.messages[i].g;
    m["b"] = s.messages[i].b;
    m["enabled"] = s.messages[i].enabled;
  }

  doc["speed"] = s.speed;
  doc["brightness"] = s.brightness;
  doc["panel_cols"] = s.panelCols;
  doc["wifi_mode"] = _wifiService.modeString();
  doc["ip"] = _wifiService.ip();
  doc["wifi_ssid"] = s.wifiSsid;
  doc["wifi_password"] = s.wifiPassword;

  doc["rss_enabled"] = s.rssEnabled;
  doc["rss_url"] = s.rssUrl;
  doc["rss_npr_enabled"] = s.rssNprEnabled;
  doc["rss_sports_enabled"] = s.rssSportsEnabled;
  doc["rss_sports_base_url"] = s.rssSportsBaseUrl;
  JsonObject sports = doc.createNestedObject("rss_sports");
  sports["mlb"] = s.rssSportMlbEnabled;
  sports["nhl"] = s.rssSportNhlEnabled;
  sports["ncaaf"] = s.rssSportNcaafEnabled;
  sports["nfl"] = s.rssSportNflEnabled;
  sports["nba"] = s.rssSportNbaEnabled;
  sports["big10"] = s.rssSportBig10Enabled;

  if (_rssRuntime != nullptr) {
    doc["rss_source_count"] = _rssRuntime->sourceCount();
    JsonArray sourceArray = doc.createNestedArray("rss_sources");
    const RssSource* sources = _rssRuntime->sources();
    for (size_t i = 0; i < _rssRuntime->sourceCount(); i++) {
      JsonObject source = sourceArray.createNestedObject();
      source["name"] = sources[i].name;
      source["url"] = sources[i].url;
      source["enabled"] = sources[i].enabled;

      RssCacheMetadata meta = {};
      const bool hasMeta = _rssRuntime->sourceMetadata(i, meta);
      source["cache_valid"] = hasMeta && meta.valid;
      source["cache_item_count"] = hasMeta ? meta.itemCount : 0;
      source["cache_updated_epoch"] = hasMeta ? meta.updatedEpoch : 0;
    }
  } else {
    doc["rss_source_count"] = 0;
    doc.createNestedArray("rss_sources");
  }

  sendJson(doc, 200);
}

void WebService::handleMessages() {
  DynamicJsonDocument doc(4096);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }

  JsonArray arr = doc["messages"].as<JsonArray>();
  if (arr.isNull()) {
    sendError("Missing 'messages' array");
    return;
  }

  AppSettings& s = _store.mutableSettings();
  size_t i = 0;
  for (JsonObject m : arr) {
    if (i >= APP_MAX_MESSAGES) break;
    strlcpy(s.messages[i].text, m["text"] | s.messages[i].text,
            sizeof(s.messages[i].text));
    s.messages[i].r = static_cast<uint8_t>(m["r"] | s.messages[i].r);
    s.messages[i].g = static_cast<uint8_t>(m["g"] | s.messages[i].g);
    s.messages[i].b = static_cast<uint8_t>(m["b"] | s.messages[i].b);
    s.messages[i].enabled = m["enabled"] | s.messages[i].enabled;
    i++;
  }

  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Messages updated");
}

void WebService::handleText() {
  DynamicJsonDocument doc(1024);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }

  const char* text = doc["text"];
  if (text == nullptr) {
    sendError("Missing 'text' field");
    return;
  }

  AppSettings& s = _store.mutableSettings();
  strlcpy(s.messages[0].text, text, sizeof(s.messages[0].text));
  s.messages[0].enabled = true;
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Text updated");
}

void WebService::handleColor() {
  DynamicJsonDocument doc(1024);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }

  if (!doc["r"].is<uint8_t>() && !doc["r"].is<int>()) {
    sendError("Missing r/g/b fields");
    return;
  }

  AppSettings& s = _store.mutableSettings();
  s.messages[0].r = static_cast<uint8_t>(doc["r"] | s.messages[0].r);
  s.messages[0].g = static_cast<uint8_t>(doc["g"] | s.messages[0].g);
  s.messages[0].b = static_cast<uint8_t>(doc["b"] | s.messages[0].b);
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Color updated");
}

void WebService::handleSpeed() {
  DynamicJsonDocument doc(512);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }
  AppSettings& s = _store.mutableSettings();
  s.speed = static_cast<uint8_t>(doc["speed"] | s.speed);
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Speed updated");
}

void WebService::handleBrightness() {
  DynamicJsonDocument doc(512);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }
  AppSettings& s = _store.mutableSettings();
  s.brightness = static_cast<uint8_t>(doc["brightness"] | s.brightness);
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Brightness updated");
}

void WebService::handleAppearance() {
  DynamicJsonDocument doc(1024);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }
  AppSettings& s = _store.mutableSettings();
  if (!doc["speed"].isNull()) {
    s.speed = static_cast<uint8_t>(doc["speed"] | s.speed);
  }
  if (!doc["brightness"].isNull()) {
    s.brightness = static_cast<uint8_t>(doc["brightness"] | s.brightness);
  }
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Appearance updated");
}

void WebService::handleWifi() {
  DynamicJsonDocument doc(1024);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }
  const char* ssid = doc["ssid"];
  if (ssid == nullptr) {
    sendError("Missing 'ssid' field");
    return;
  }

  AppSettings& s = _store.mutableSettings();
  strlcpy(s.wifiSsid, ssid, sizeof(s.wifiSsid));
  strlcpy(s.wifiPassword, doc["password"] | "", sizeof(s.wifiPassword));
  _store.save();
  if (_onWifiConnectRequested) _onWifiConnectRequested();
  sendStatusMessage("Connecting to WiFi...");
}

void WebService::handleAdvanced() {
  DynamicJsonDocument doc(1024);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }
  AppSettings& s = _store.mutableSettings();
  uint8_t panel = static_cast<uint8_t>(doc["panel_cols"] | s.panelCols);
  if (panel == 32 || panel == 64 || panel == 96 || panel == 128) {
    s.panelCols = panel;
  }
  _store.save();
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("Advanced settings updated");
}

void WebService::handleRss() {
  DynamicJsonDocument doc(2048);
  if (!parseBodyJson(doc)) {
    sendError("Invalid JSON");
    return;
  }

  AppSettings& s = _store.mutableSettings();
  if (!doc["enabled"].isNull()) s.rssEnabled = doc["enabled"] | s.rssEnabled;
  if (!doc["rss_enabled"].isNull()) s.rssEnabled = doc["rss_enabled"] | s.rssEnabled;

  if (!doc["url"].isNull()) strlcpy(s.rssUrl, doc["url"] | s.rssUrl, sizeof(s.rssUrl));
  if (!doc["rss_url"].isNull()) strlcpy(s.rssUrl, doc["rss_url"] | s.rssUrl, sizeof(s.rssUrl));

  if (!doc["npr_enabled"].isNull()) s.rssNprEnabled = doc["npr_enabled"] | s.rssNprEnabled;
  if (!doc["rss_npr_enabled"].isNull()) s.rssNprEnabled = doc["rss_npr_enabled"] | s.rssNprEnabled;

  if (!doc["sports_enabled"].isNull()) s.rssSportsEnabled = doc["sports_enabled"] | s.rssSportsEnabled;
  if (!doc["rss_sports_enabled"].isNull()) s.rssSportsEnabled = doc["rss_sports_enabled"] | s.rssSportsEnabled;

  if (!doc["sports_base_url"].isNull()) {
    strlcpy(s.rssSportsBaseUrl, doc["sports_base_url"] | s.rssSportsBaseUrl, sizeof(s.rssSportsBaseUrl));
  }
  if (!doc["rss_sports_base_url"].isNull()) {
    strlcpy(s.rssSportsBaseUrl, doc["rss_sports_base_url"] | s.rssSportsBaseUrl, sizeof(s.rssSportsBaseUrl));
  }

  JsonObject sports = doc["sports"].as<JsonObject>();
  if (!sports.isNull()) {
    if (!sports["mlb"].isNull()) s.rssSportMlbEnabled = sports["mlb"] | s.rssSportMlbEnabled;
    if (!sports["nhl"].isNull()) s.rssSportNhlEnabled = sports["nhl"] | s.rssSportNhlEnabled;
    if (!sports["ncaaf"].isNull()) s.rssSportNcaafEnabled = sports["ncaaf"] | s.rssSportNcaafEnabled;
    if (!sports["nfl"].isNull()) s.rssSportNflEnabled = sports["nfl"] | s.rssSportNflEnabled;
    if (!sports["nba"].isNull()) s.rssSportNbaEnabled = sports["nba"] | s.rssSportNbaEnabled;
    if (!sports["big10"].isNull()) s.rssSportBig10Enabled = sports["big10"] | s.rssSportBig10Enabled;
  }

  if (!doc["rss_sport_mlb_enabled"].isNull()) s.rssSportMlbEnabled = doc["rss_sport_mlb_enabled"] | s.rssSportMlbEnabled;
  if (!doc["rss_sport_nhl_enabled"].isNull()) s.rssSportNhlEnabled = doc["rss_sport_nhl_enabled"] | s.rssSportNhlEnabled;
  if (!doc["rss_sport_ncaaf_enabled"].isNull()) s.rssSportNcaafEnabled = doc["rss_sport_ncaaf_enabled"] | s.rssSportNcaafEnabled;
  if (!doc["rss_sport_nfl_enabled"].isNull()) s.rssSportNflEnabled = doc["rss_sport_nfl_enabled"] | s.rssSportNflEnabled;
  if (!doc["rss_sport_nba_enabled"].isNull()) s.rssSportNbaEnabled = doc["rss_sport_nba_enabled"] | s.rssSportNbaEnabled;
  if (!doc["rss_sport_big10_enabled"].isNull()) s.rssSportBig10Enabled = doc["rss_sport_big10_enabled"] | s.rssSportBig10Enabled;

  if (!_store.save()) {
    sendError("Failed to persist RSS settings", 500);
    return;
  }
  if (_onSettingsChanged) _onSettingsChanged(s);
  sendStatusMessage("RSS settings updated");
}

void WebService::handleFactoryReset() {
  if (_onFactoryResetRequested) {
    _onFactoryResetRequested();
  } else {
    _store.factoryReset();
  }
  sendStatusMessage("Factory reset triggered");
}

void WebService::handleNotFound() const {
  if (_wifiService.mode() == WifiRuntimeMode::AP) {
    _server->sendHeader("Location", String("http://") + _wifiService.ip() + "/",
                        true);
    _server->send(302, "text/plain", "");
    return;
  }
  _server->send(404, "text/plain", "Not found");
}
