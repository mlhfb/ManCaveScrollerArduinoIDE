#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "SettingsStore.h"
#include "WifiService.h"

class WebServer;
class RssRuntime;

class WebService {
public:
  using SettingsChangedCallback = void (*)(const AppSettings&);
  using VoidCallback = void (*)();

  WebService(SettingsStore& store, WifiService& wifiService);
  ~WebService();

  void begin(uint16_t port = 80);
  void stop();
  void tick();
  bool isRunning() const;

  void setOnSettingsChanged(SettingsChangedCallback cb);
  void setOnWifiConnectRequested(VoidCallback cb);
  void setOnFactoryResetRequested(VoidCallback cb);
  void setRssRuntime(RssRuntime* rssRuntime);

private:
  void registerRoutes();
  String readBody() const;
  bool parseBodyJson(JsonDocument& doc) const;
  void sendJson(const JsonDocument& doc, int code = 200) const;
  void sendStatusMessage(const char* status) const;
  void sendError(const char* message, int code = 400) const;

  void handleRoot() const;
  void handleStatus() const;
  void handleMessages();
  void handleText();
  void handleColor();
  void handleSpeed();
  void handleBrightness();
  void handleAppearance();
  void handleWifi();
  void handleAdvanced();
  void handleRss();
  void handleFactoryReset();
  void handleNotFound() const;

  WebServer* _server;
  SettingsStore& _store;
  WifiService& _wifiService;
  RssRuntime* _rssRuntime;
  SettingsChangedCallback _onSettingsChanged;
  VoidCallback _onWifiConnectRequested;
  VoidCallback _onFactoryResetRequested;
};

#endif
