#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>

#include "WifiService.h"

enum class OtaState {
  Idle,
  Checking,
  Available,
  Downloading,
  Verifying,
  Installing,
  Rebooting,
  Error,
};

struct OtaStatusSnapshot {
  OtaState state;
  String currentVersion;
  String availableVersion;
  uint8_t progressPercent;
  String lastError;
};

class OtaService {
public:
  explicit OtaService(WifiService& wifiService);

  void begin();
  void tick();

  bool checkForUpdate(const char* manifestUrl = nullptr);
  bool startUpdate(bool confirm);

  OtaStatusSnapshot status() const;
  const char* stateString() const;
  bool hasUpdateAvailable() const;

private:
  struct OtaManifest {
    String version;
    String firmwareUrl;
    size_t firmwareSize;
    String firmwareSha256;
  };

  void setState(OtaState state);
  void setError(const String& message);
  bool parseManifest(const String& payload, OtaManifest& outManifest) const;
  bool fetchStringFromUrl(const String& url, String& outPayload,
                          uint32_t* outHttpCode = nullptr);
  bool updateFromManifest();
  int compareVersions(const String& lhs, const String& rhs) const;
  bool isLikelyHex64(const String& value) const;

  WifiService& _wifiService;
  OtaState _state;
  String _manifestUrl;
  OtaManifest _availableManifest;
  bool _hasAvailableManifest;
  uint8_t _progressPercent;
  String _lastError;
  bool _rebootPending;
  uint32_t _rebootAfterMs;
};

#endif
