#include "OtaService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <memory>

#include "AppConfig.h"

namespace {
constexpr uint32_t kHttpTimeoutMs = 15000;
constexpr uint32_t kDownloadStallTimeoutMs = 8000;
constexpr uint32_t kRebootDelayMs = 1200;
constexpr size_t kDownloadBufferSize = 1024;

bool startsWithHttps(const String& url) { return url.startsWith("https://"); }

String trimAndLower(const String& value) {
  String out = value;
  out.trim();
  out.toLowerCase();
  return out;
}

String sha256ToHex(const uint8_t hash[32]) {
  static const char* kHex = "0123456789abcdef";
  char out[65] = {};
  for (size_t i = 0; i < 32; i++) {
    out[i * 2] = kHex[(hash[i] >> 4) & 0x0F];
    out[(i * 2) + 1] = kHex[hash[i] & 0x0F];
  }
  out[64] = '\0';
  return String(out);
}

int nextVersionToken(const String& value, int& index) {
  if (index < 0 || index >= static_cast<int>(value.length())) {
    return 0;
  }

  const int dotIndex = value.indexOf('.', index);
  int end = dotIndex >= 0 ? dotIndex : value.length();
  if (end <= index) {
    index = end + 1;
    return 0;
  }

  long parsed = 0;
  bool anyDigit = false;
  for (int i = index; i < end; i++) {
    const char c = value[i];
    if (c < '0' || c > '9') {
      break;
    }
    anyDigit = true;
    parsed = (parsed * 10L) + static_cast<long>(c - '0');
    if (parsed > 65535L) {
      parsed = 65535L;
      break;
    }
  }

  index = end + 1;
  if (!anyDigit) {
    return 0;
  }
  return static_cast<int>(parsed);
}
}  // namespace

OtaService::OtaService(WifiService& wifiService)
    : _wifiService(wifiService),
      _state(OtaState::Idle),
      _manifestUrl(APP_OTA_MANIFEST_URL),
      _availableManifest(),
      _hasAvailableManifest(false),
      _progressPercent(0),
      _lastError(),
      _rebootPending(false),
      _rebootAfterMs(0) {}

void OtaService::begin() {
  _state = OtaState::Idle;
  _progressPercent = 0;
  _lastError = "";
  _rebootPending = false;
  _rebootAfterMs = 0;
}

void OtaService::tick() {
  if (!_rebootPending) {
    return;
  }

  const int32_t remaining = static_cast<int32_t>(_rebootAfterMs - millis());
  if (remaining <= 0) {
    ESP.restart();
  }
}

void OtaService::setState(OtaState state) { _state = state; }

void OtaService::setError(const String& message) {
  _lastError = message;
  _progressPercent = 0;
  _state = OtaState::Error;
}

bool OtaService::isLikelyHex64(const String& value) const {
  if (value.length() != 64) {
    return false;
  }

  for (size_t i = 0; i < value.length(); i++) {
    if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }
  return true;
}

int OtaService::compareVersions(const String& lhs, const String& rhs) const {
  String left = lhs;
  String right = rhs;
  left.trim();
  right.trim();

  const int leftSuffix = left.indexOf('-');
  const int rightSuffix = right.indexOf('-');
  if (leftSuffix >= 0) {
    left = left.substring(0, leftSuffix);
  }
  if (rightSuffix >= 0) {
    right = right.substring(0, rightSuffix);
  }

  int leftIndex = 0;
  int rightIndex = 0;
  for (int i = 0; i < 4; i++) {
    const int l = nextVersionToken(left, leftIndex);
    const int r = nextVersionToken(right, rightIndex);
    if (l < r) {
      return -1;
    }
    if (l > r) {
      return 1;
    }
  }
  return 0;
}

bool OtaService::parseManifest(const String& payload,
                               OtaManifest& outManifest) const {
  DynamicJsonDocument doc(3072);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    return false;
  }

  const char* version = doc["version"];
  JsonObject firmware = doc["firmware"].as<JsonObject>();
  if (version == nullptr || firmware.isNull()) {
    return false;
  }

  const char* url = firmware["url"];
  if (url == nullptr) {
    return false;
  }

  const size_t size = firmware["size"] | 0;
  String sha256 = firmware["sha256"] | "";
  sha256 = trimAndLower(sha256);
  if (size == 0 || !isLikelyHex64(sha256)) {
    return false;
  }

  outManifest.version = String(version);
  outManifest.version.trim();
  outManifest.firmwareUrl = String(url);
  outManifest.firmwareUrl.trim();
  outManifest.firmwareSize = size;
  outManifest.firmwareSha256 = sha256;
  return outManifest.version.length() > 0 && outManifest.firmwareUrl.length() > 0;
}

bool OtaService::fetchStringFromUrl(const String& url, String& outPayload,
                                    uint32_t* outHttpCode) {
  outPayload = "";
  if (outHttpCode != nullptr) {
    *outHttpCode = 0;
  }

  std::unique_ptr<WiFiClient> client;
  if (startsWithHttps(url)) {
    WiFiClientSecure* secureClient = new WiFiClientSecure();
    if (secureClient == nullptr) {
      setError("OTA HTTP secure client allocation failed");
      return false;
    }
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  if (!http.begin(*client, url)) {
    setError("OTA HTTP begin failed");
    return false;
  }
  http.setTimeout(kHttpTimeoutMs);
  const int code = http.GET();
  if (outHttpCode != nullptr) {
    *outHttpCode = static_cast<uint32_t>(code);
  }
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("OTA HTTP GET failed: ") + code);
    return false;
  }

  outPayload = http.getString();
  http.end();
  if (outPayload.length() == 0) {
    setError("OTA response body empty");
    return false;
  }
  return true;
}

bool OtaService::checkForUpdate(const char* manifestUrl) {
  _lastError = "";
  _progressPercent = 0;
  _hasAvailableManifest = false;
  _availableManifest = OtaManifest();

  const String resolvedManifestUrl =
      (manifestUrl != nullptr && manifestUrl[0] != '\0')
          ? String(manifestUrl)
          : String(APP_OTA_MANIFEST_URL);
  _manifestUrl = resolvedManifestUrl;
  _manifestUrl.trim();

  if (_manifestUrl.length() == 0) {
    setError("OTA manifest URL not configured");
    return false;
  }
  if (!_wifiService.isConnected()) {
    setError("WiFi STA connection required for OTA check");
    return false;
  }

  setState(OtaState::Checking);
  String payload;
  if (!fetchStringFromUrl(_manifestUrl, payload)) {
    return false;
  }

  OtaManifest manifest;
  if (!parseManifest(payload, manifest)) {
    setError("OTA manifest parse/validation failed");
    return false;
  }

  const int cmp = compareVersions(manifest.version, APP_FIRMWARE_VERSION);
  if (cmp <= 0) {
    setState(OtaState::Idle);
    return true;
  }

  _availableManifest = manifest;
  _hasAvailableManifest = true;
  setState(OtaState::Available);
  return true;
}

bool OtaService::updateFromManifest() {
  if (!_wifiService.isConnected()) {
    setError("WiFi STA connection required for OTA update");
    return false;
  }

  const esp_partition_t* nextPartition =
      esp_ota_get_next_update_partition(nullptr);
  if (nextPartition == nullptr) {
    setError("Inactive OTA partition not found");
    return false;
  }
  if (_availableManifest.firmwareSize > nextPartition->size) {
    setError("Firmware larger than inactive OTA slot");
    return false;
  }

  std::unique_ptr<WiFiClient> client;
  if (startsWithHttps(_availableManifest.firmwareUrl)) {
    WiFiClientSecure* secureClient = new WiFiClientSecure();
    if (secureClient == nullptr) {
      setError("OTA secure client allocation failed");
      return false;
    }
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  if (!http.begin(*client, _availableManifest.firmwareUrl)) {
    setError("OTA firmware HTTP begin failed");
    return false;
  }
  http.setTimeout(kHttpTimeoutMs);

  setState(OtaState::Downloading);
  _progressPercent = 1;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("OTA firmware HTTP GET failed: ") + code);
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength > 0 &&
      static_cast<size_t>(contentLength) != _availableManifest.firmwareSize) {
    http.end();
    setError("OTA firmware size mismatch");
    return false;
  }

  if (!Update.begin(_availableManifest.firmwareSize, U_FLASH)) {
    http.end();
    setError(String("Update.begin failed: ") + Update.errorString());
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[kDownloadBufferSize];
  size_t totalWritten = 0;
  uint32_t lastChunkMs = millis();

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts_ret(&shaCtx, 0);

  bool failed = false;
  while (totalWritten < _availableManifest.firmwareSize) {
    size_t available = stream->available();
    if (available == 0) {
      if (!http.connected()) {
        failed = true;
        break;
      }
      if ((millis() - lastChunkMs) > kDownloadStallTimeoutMs) {
        failed = true;
        break;
      }
      delay(1);
      continue;
    }

    size_t toRead = available;
    if (toRead > kDownloadBufferSize) {
      toRead = kDownloadBufferSize;
    }
    const size_t remaining = _availableManifest.firmwareSize - totalWritten;
    if (toRead > remaining) {
      toRead = remaining;
    }

    const size_t readCount = stream->readBytes(buffer, toRead);
    if (readCount == 0) {
      delay(1);
      continue;
    }

    lastChunkMs = millis();
    const size_t written = Update.write(buffer, readCount);
    if (written != readCount) {
      failed = true;
      break;
    }

    mbedtls_sha256_update_ret(&shaCtx, buffer, readCount);
    totalWritten += readCount;
    _progressPercent = static_cast<uint8_t>(
        (totalWritten * 90U) / _availableManifest.firmwareSize);
    if (_progressPercent < 1) {
      _progressPercent = 1;
    }
  }

  uint8_t hash[32] = {};
  mbedtls_sha256_finish_ret(&shaCtx, hash);
  mbedtls_sha256_free(&shaCtx);
  http.end();

  if (failed || totalWritten != _availableManifest.firmwareSize) {
    Update.abort();
    setError("OTA download/write failed");
    return false;
  }

  setState(OtaState::Verifying);
  _progressPercent = 95;
  const String actualHash = sha256ToHex(hash);
  if (!actualHash.equalsIgnoreCase(_availableManifest.firmwareSha256)) {
    Update.abort();
    setError("OTA SHA-256 mismatch");
    return false;
  }

  if (!Update.end(true)) {
    setError(String("Update.end failed: ") + Update.errorString());
    return false;
  }
  if (!Update.isFinished()) {
    setError("OTA update not finished");
    return false;
  }

  setState(OtaState::Installing);
  _progressPercent = 100;
  _hasAvailableManifest = false;
  _availableManifest = OtaManifest();

  setState(OtaState::Rebooting);
  _rebootPending = true;
  _rebootAfterMs = millis() + kRebootDelayMs;
  return true;
}

bool OtaService::startUpdate(bool confirm) {
  if (!confirm) {
    setError("OTA update requires confirm=true");
    return false;
  }
  if (!_hasAvailableManifest) {
    setError("No OTA update available");
    return false;
  }
  if (_state == OtaState::Downloading || _state == OtaState::Verifying ||
      _state == OtaState::Installing || _state == OtaState::Rebooting) {
    setError("OTA update already in progress");
    return false;
  }

  _lastError = "";
  _progressPercent = 0;
  return updateFromManifest();
}

OtaStatusSnapshot OtaService::status() const {
  OtaStatusSnapshot snapshot = {};
  snapshot.state = _state;
  snapshot.currentVersion = APP_FIRMWARE_VERSION;
  snapshot.availableVersion = _hasAvailableManifest ? _availableManifest.version : "";
  snapshot.progressPercent = _progressPercent;
  snapshot.lastError = _lastError;
  return snapshot;
}

bool OtaService::hasUpdateAvailable() const { return _hasAvailableManifest; }

const char* OtaService::stateString() const {
  switch (_state) {
    case OtaState::Idle:
      return "idle";
    case OtaState::Checking:
      return "checking";
    case OtaState::Available:
      return "available";
    case OtaState::Downloading:
      return "downloading";
    case OtaState::Verifying:
      return "verifying";
    case OtaState::Installing:
      return "installing";
    case OtaState::Rebooting:
      return "rebooting";
    case OtaState::Error:
    default:
      return "error";
  }
}
