#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "WifiService.h"

class OtaService {
public:
  OtaService(WifiService& wifiService);

  void begin(const char* currentVersion);
  void setDefaultManifestUrl(const char* manifestUrl);

  bool checkForUpdate(const char* manifestUrl = nullptr);
  bool installAvailableUpdate();

  bool hasPendingUpdate() const;
  bool hasError() const;
  const char* lastError() const;

  const char* currentVersion() const;
  const char* availableVersion() const;
  const char* stateString() const;
  const char* defaultManifestUrl() const;
  const char* lastManifestUrl() const;
  const char* availableFirmwareUrl() const;

  void appendStatus(JsonObject obj) const;

private:
  enum class OtaState : uint8_t {
    Idle,
    Checking,
    UpToDate,
    Available,
    Downloading,
    Installing,
    RebootRequired,
    Error,
  };

  static bool isVersionNewer(const String& candidate, const String& current);
  static int nextVersionToken(const String& value, size_t& pos);
  static String normalizeManifestUrl(String url);
  static String normalizeHttpUrl(String url);
  static String normalizeMd5Hex(String value);
  bool fetchManifest(const String& manifestUrl);
  void setError(const String& message);
  void clearError();

  WifiService& _wifiService;
  OtaState _state;
  String _currentVersion;
  String _availableVersion;
  String _lastManifestUrl;
  String _defaultManifestUrl;
  String _firmwareUrl;
  String _firmwareMd5;
  String _firmwareSha256;
  uint32_t _firmwareSize;
  bool _hasPendingUpdate;
  String _lastError;
  uint32_t _lastCheckedMs;
};

#endif
