#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>

#include "AppTypes.h"

class DNSServer;

enum class WifiRuntimeMode {
  Off,
  AP,
  StaConnecting,
  StaConnected,
};

class WifiService {
public:
  WifiService();
  ~WifiService();

  void begin();
  void tick();

  bool startForSettings(const AppSettings& settings);
  bool connectSta(const char* ssid, const char* password,
                  uint32_t timeoutMs = 15000, uint8_t maxRetries = 3);
  void startAp(const char* ssid = "ManCave", const char* password = "");
  void stopWifi();

  bool enterConfigMode(const AppSettings& settings);
  void exitConfigMode(bool radioOffAfterExit);

  WifiRuntimeMode mode() const;
  bool isConnected() const;
  String ip() const;
  const char* modeString() const;

private:
  void startCaptiveDns(IPAddress apIp);
  void stopCaptiveDns();

  WifiRuntimeMode _mode;
  DNSServer* _dns;
  bool _dnsRunning;
};

#endif
