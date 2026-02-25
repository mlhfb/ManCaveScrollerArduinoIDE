#include "WifiService.h"

#include <DNSServer.h>
#include <WiFi.h>

namespace {
constexpr byte kDnsPort = 53;
}  // namespace

WifiService::WifiService()
    : _mode(WifiRuntimeMode::Off), _dns(nullptr), _dnsRunning(false) {}

WifiService::~WifiService() { stopCaptiveDns(); }

void WifiService::begin() { WiFi.persistent(false); }

void WifiService::tick() {
  if (_dnsRunning && _dns != nullptr) {
    _dns->processNextRequest();
  }
}

bool WifiService::startForSettings(const AppSettings& settings) {
  if (settings.wifiSsid[0] == '\0') {
    startAp();
    return false;
  }
  bool connected = connectSta(settings.wifiSsid, settings.wifiPassword);
  if (!connected) {
    startAp();
  }
  return connected;
}

bool WifiService::connectSta(const char* ssid, const char* password,
                             uint32_t timeoutMs, uint8_t maxRetries) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }

  stopCaptiveDns();
  WiFi.mode(WIFI_STA);
  _mode = WifiRuntimeMode::StaConnecting;

  uint8_t attempt = 0;
  while (attempt < maxRetries) {
    WiFi.begin(ssid, password);
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - started) < timeoutMs) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      _mode = WifiRuntimeMode::StaConnected;
      return true;
    }

    WiFi.disconnect(true, true);
    attempt++;
    delay(250);
  }

  _mode = WifiRuntimeMode::Off;
  return false;
}

void WifiService::startAp(const char* ssid, const char* password) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  _mode = WifiRuntimeMode::AP;
  startCaptiveDns(WiFi.softAPIP());
}

void WifiService::stopWifi() {
  stopCaptiveDns();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  _mode = WifiRuntimeMode::Off;
}

bool WifiService::enterConfigMode(const AppSettings& settings) {
  if (settings.wifiSsid[0] != '\0' &&
      connectSta(settings.wifiSsid, settings.wifiPassword)) {
    return true;
  }

  startAp();
  return true;
}

void WifiService::exitConfigMode(bool radioOffAfterExit) {
  if (radioOffAfterExit) {
    stopWifi();
  }
}

WifiRuntimeMode WifiService::mode() const { return _mode; }

bool WifiService::isConnected() const { return WiFi.status() == WL_CONNECTED; }

String WifiService::ip() const {
  if (_mode == WifiRuntimeMode::AP) {
    return WiFi.softAPIP().toString();
  }
  if (_mode == WifiRuntimeMode::StaConnected ||
      _mode == WifiRuntimeMode::StaConnecting) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

const char* WifiService::modeString() const {
  switch (_mode) {
    case WifiRuntimeMode::AP:
      return "AP";
    case WifiRuntimeMode::StaConnecting:
      return "Connecting";
    case WifiRuntimeMode::StaConnected:
      return "STA";
    case WifiRuntimeMode::Off:
    default:
      return "Off";
  }
}

void WifiService::startCaptiveDns(IPAddress apIp) {
  stopCaptiveDns();
  _dns = new DNSServer();
  if (_dns == nullptr) {
    return;
  }

  _dns->setErrorReplyCode(DNSReplyCode::NoError);
  _dns->start(kDnsPort, "*", apIp);
  _dnsRunning = true;
}

void WifiService::stopCaptiveDns() {
  if (_dns != nullptr) {
    _dns->stop();
    delete _dns;
    _dns = nullptr;
  }
  _dnsRunning = false;
}
