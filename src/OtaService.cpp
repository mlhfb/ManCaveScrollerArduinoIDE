#include "OtaService.h"

#include <memory>

#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

#ifndef APP_OTA_MANIFEST_URL
#define APP_OTA_MANIFEST_URL ""
#endif

namespace {
constexpr uint32_t kHttpTimeoutMs = 15000;
const char* kSettingsPath = "/config/settings.json";
const char* kSettingsBackupNamespace = "ota_settings";
const char* kBackupPendingKey = "pending";
const char* kBackupLengthKey = "len";
const char* kBackupBlobKey = "blob";
const char* kOtaMetaNamespace = "ota_meta";
const char* kLittleFsVersionKey = "fs_ver";
}

OtaService::OtaService(WifiService& wifiService)
    : _wifiService(wifiService),
      _state(OtaState::Idle),
      _currentVersion("unknown"),
      _availableVersion(""),
      _lastManifestUrl(""),
      _defaultManifestUrl(APP_OTA_MANIFEST_URL),
      _firmwareUrl(""),
      _firmwareMd5(""),
      _firmwareSha256(""),
      _firmwareSize(0),
      _hasPendingFirmwareUpdate(false),
      _currentLittleFsVersion(""),
      _availableLittleFsVersion(""),
      _littleFsUrl(""),
      _littleFsMd5(""),
      _littleFsSha256(""),
      _littleFsSize(0),
      _hasPendingLittleFsUpdate(false),
      _lastError(""),
      _lastCheckedMs(0) {}

void OtaService::begin(const char* currentVersion) {
  if (currentVersion != nullptr && currentVersion[0] != '\0') {
    _currentVersion = currentVersion;
  } else {
    _currentVersion = "unknown";
  }
  loadCurrentLittleFsVersion();
  _availableVersion = "";
  _availableLittleFsVersion = "";
  _firmwareUrl = "";
  _firmwareMd5 = "";
  _firmwareSha256 = "";
  _firmwareSize = 0;
  _littleFsUrl = "";
  _littleFsMd5 = "";
  _littleFsSha256 = "";
  _littleFsSize = 0;
  _hasPendingFirmwareUpdate = false;
  _hasPendingLittleFsUpdate = false;
  setDefaultManifestUrl(_defaultManifestUrl.c_str());
  _state = OtaState::Idle;
  _lastError = "";
}

void OtaService::setDefaultManifestUrl(const char* manifestUrl) {
  String url = manifestUrl != nullptr ? manifestUrl : "";
  url = normalizeManifestUrl(url);
  if (url.length() == 0) {
    _defaultManifestUrl = normalizeManifestUrl(APP_OTA_MANIFEST_URL);
  } else {
    _defaultManifestUrl = url;
  }
}

bool OtaService::checkForUpdate(const char* manifestUrl) {
  String url = manifestUrl != nullptr ? manifestUrl : "";
  url = normalizeManifestUrl(url);
  if (url.length() == 0) {
    url = normalizeManifestUrl(_defaultManifestUrl);
  }

  if (url.length() == 0) {
    setError("OTA manifest URL is not configured");
    return false;
  }
  if (!_wifiService.isConnected()) {
    setError("WiFi STA connection required for OTA check");
    return false;
  }

  _state = OtaState::Checking;
  _lastCheckedMs = millis();
  if (!fetchManifest(url)) {
    return false;
  }

  _lastManifestUrl = url;
  _hasPendingFirmwareUpdate =
      _firmwareUrl.length() > 0 &&
      isVersionNewer(_availableVersion, _currentVersion);
  _hasPendingLittleFsUpdate =
      _littleFsUrl.length() > 0 &&
      isVersionNewer(_availableLittleFsVersion, _currentLittleFsVersion);
  _state = hasPendingUpdate() ? OtaState::Available : OtaState::UpToDate;
  clearError();
  return true;
}

bool OtaService::installAvailableUpdate() {
  if (_hasPendingFirmwareUpdate) {
    return installAvailableFirmwareUpdate();
  }
  if (_hasPendingLittleFsUpdate) {
    return installAvailableLittleFsUpdate();
  }
  setError("No OTA payload is pending");
  return false;
}

bool OtaService::installAvailableFirmwareUpdate() {
  if (!_wifiService.isConnected()) {
    setError("WiFi STA connection required for OTA install");
    return false;
  }
  const String firmwareUrl = normalizeHttpUrl(_firmwareUrl);
  if (firmwareUrl.length() == 0) {
    setError("No OTA firmware URL is available");
    return false;
  }

  _state = OtaState::Downloading;
  clearError();

  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  bool beginOk = false;
  if (firmwareUrl.startsWith("https://")) {
    beginOk = http.begin(secureClient, firmwareUrl);
  } else {
    beginOk = http.begin(plainClient, firmwareUrl);
  }
  if (!beginOk) {
    setError("Failed to begin firmware download");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("Firmware HTTP error: ") + code);
    return false;
  }

  const int contentLength = http.getSize();
  if (_firmwareSize > 0 && contentLength > 0 &&
      static_cast<uint32_t>(contentLength) != _firmwareSize) {
    http.end();
    setError("Firmware size mismatch versus manifest");
    return false;
  }

  size_t updateSize = UPDATE_SIZE_UNKNOWN;
  if (_firmwareSize > 0) {
    updateSize = _firmwareSize;
  } else if (contentLength > 0) {
    updateSize = static_cast<size_t>(contentLength);
  }

  _state = OtaState::Installing;
  if (!Update.begin(updateSize)) {
    http.end();
    setError(String("Update begin failed: ") + Update.errorString());
    return false;
  }

  if (_firmwareMd5.length() == 32) {
    if (!Update.setMD5(_firmwareMd5.c_str())) {
      Update.abort();
      http.end();
      setError("Failed to set OTA MD5");
      return false;
    }
  }

  WiFiClient& stream = http.getStream();
  const size_t written = Update.writeStream(stream);
  if (updateSize != UPDATE_SIZE_UNKNOWN && written != updateSize) {
    Update.abort();
    http.end();
    setError("Incomplete OTA write");
    return false;
  }

  const bool endOk = Update.end(true);
  http.end();
  if (!endOk) {
    setError(String("Update end failed: ") + Update.errorString());
    return false;
  }
  if (!Update.isFinished()) {
    setError("OTA did not finish");
    return false;
  }

  _state = OtaState::RebootRequired;
  _hasPendingFirmwareUpdate = false;
  clearError();
  return true;
}

bool OtaService::installAvailableLittleFsUpdate() {
  if (!_wifiService.isConnected()) {
    setError("WiFi STA connection required for OTA install");
    return false;
  }
  const String littleFsUrl = normalizeHttpUrl(_littleFsUrl);
  if (littleFsUrl.length() == 0) {
    setError("No OTA littlefs URL is available");
    return false;
  }
  if (!backupSettingsForLittleFsUpdate()) {
    setError("Failed to backup settings before littlefs update");
    return false;
  }

  _state = OtaState::Downloading;
  clearError();

  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  bool beginOk = false;
  if (littleFsUrl.startsWith("https://")) {
    beginOk = http.begin(secureClient, littleFsUrl);
  } else {
    beginOk = http.begin(plainClient, littleFsUrl);
  }
  if (!beginOk) {
    setError("Failed to begin littlefs download");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("LittleFS HTTP error: ") + code);
    return false;
  }

  const int contentLength = http.getSize();
  if (_littleFsSize > 0 && contentLength > 0 &&
      static_cast<uint32_t>(contentLength) != _littleFsSize) {
    http.end();
    setError("LittleFS size mismatch versus manifest");
    return false;
  }

  size_t updateSize = UPDATE_SIZE_UNKNOWN;
  if (_littleFsSize > 0) {
    updateSize = _littleFsSize;
  } else if (contentLength > 0) {
    updateSize = static_cast<size_t>(contentLength);
  }

  LittleFS.end();

  _state = OtaState::Installing;
  if (!Update.begin(updateSize, U_SPIFFS, -1, LOW, "littlefs")) {
    http.end();
    setError(String("LittleFS begin failed: ") + Update.errorString());
    return false;
  }

  if (_littleFsMd5.length() == 32) {
    if (!Update.setMD5(_littleFsMd5.c_str())) {
      Update.abort();
      http.end();
      setError("Failed to set littlefs MD5");
      return false;
    }
  }

  WiFiClient& stream = http.getStream();
  const size_t written = Update.writeStream(stream);
  if (updateSize != UPDATE_SIZE_UNKNOWN && written != updateSize) {
    Update.abort();
    http.end();
    setError("Incomplete littlefs write");
    return false;
  }

  const bool endOk = Update.end(true);
  http.end();
  if (!endOk) {
    setError(String("LittleFS end failed: ") + Update.errorString());
    return false;
  }
  if (!Update.isFinished()) {
    setError("LittleFS OTA did not finish");
    return false;
  }

  if (_availableLittleFsVersion.length() > 0) {
    persistCurrentLittleFsVersion(_availableLittleFsVersion);
    _currentLittleFsVersion = _availableLittleFsVersion;
  }

  _state = OtaState::RebootRequired;
  _hasPendingLittleFsUpdate = false;
  clearError();
  return true;
}

bool OtaService::hasPendingUpdate() const {
  return _hasPendingFirmwareUpdate || _hasPendingLittleFsUpdate;
}

bool OtaService::hasPendingFirmwareUpdate() const {
  return _hasPendingFirmwareUpdate;
}

bool OtaService::hasPendingLittleFsUpdate() const {
  return _hasPendingLittleFsUpdate;
}

bool OtaService::hasError() const { return _lastError.length() > 0; }

const char* OtaService::lastError() const { return _lastError.c_str(); }

const char* OtaService::currentVersion() const { return _currentVersion.c_str(); }

const char* OtaService::availableVersion() const {
  return _availableVersion.c_str();
}

const char* OtaService::currentLittleFsVersion() const {
  return _currentLittleFsVersion.c_str();
}

const char* OtaService::availableLittleFsVersion() const {
  return _availableLittleFsVersion.c_str();
}

const char* OtaService::defaultManifestUrl() const {
  return _defaultManifestUrl.c_str();
}

const char* OtaService::lastManifestUrl() const {
  return _lastManifestUrl.c_str();
}

const char* OtaService::availableFirmwareUrl() const {
  return _firmwareUrl.c_str();
}

const char* OtaService::availableLittleFsUrl() const {
  return _littleFsUrl.c_str();
}

const char* OtaService::stateString() const {
  switch (_state) {
    case OtaState::Checking:
      return "checking";
    case OtaState::UpToDate:
      return "up_to_date";
    case OtaState::Available:
      return "available";
    case OtaState::Downloading:
      return "downloading";
    case OtaState::Installing:
      return "installing";
    case OtaState::RebootRequired:
      return "reboot_required";
    case OtaState::Error:
      return "error";
    case OtaState::Idle:
    default:
      return "idle";
  }
}

void OtaService::appendStatus(JsonObject obj) const {
  obj["state"] = stateString();
  obj["current_version"] = _currentVersion;
  obj["available_version"] = _availableVersion;
  obj["current_littlefs_version"] = _currentLittleFsVersion;
  obj["available_littlefs_version"] = _availableLittleFsVersion;
  obj["manifest_url"] =
      _lastManifestUrl.length() ? _lastManifestUrl : _defaultManifestUrl;
  obj["firmware_url"] = _firmwareUrl;
  obj["firmware_size"] = _firmwareSize;
  obj["littlefs_url"] = _littleFsUrl;
  obj["littlefs_size"] = _littleFsSize;
  obj["has_update"] = hasPendingUpdate();
  obj["has_firmware_update"] = _hasPendingFirmwareUpdate;
  obj["has_littlefs_update"] = _hasPendingLittleFsUpdate;
  obj["last_error"] = _lastError;
  obj["last_check_ms"] = _lastCheckedMs;
  obj["wifi_connected"] = _wifiService.isConnected();
}

bool OtaService::fetchManifest(const String& manifestUrl) {
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  bool beginOk = false;
  if (manifestUrl.startsWith("https://")) {
    beginOk = http.begin(secureClient, manifestUrl);
  } else {
    beginOk = http.begin(plainClient, manifestUrl);
  }
  if (!beginOk) {
    setError(String("Failed to begin manifest request: ") + manifestUrl);
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("Manifest HTTP error: ") + code);
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    setError("Manifest JSON parse failed");
    return false;
  }

  JsonObject firmware = doc["firmware"].as<JsonObject>();
  JsonObject littlefs = doc["littlefs"].as<JsonObject>();

  const char* version = doc["version"] | "";
  if (version == nullptr || version[0] == '\0') {
    setError("Manifest missing 'version'");
    return false;
  }
  _availableVersion = version;

  const char* fwUrl = firmware["url"] | "";
  _firmwareUrl = normalizeHttpUrl(fwUrl != nullptr ? fwUrl : "");
  _firmwareSize = firmware["size"] | 0;
  _firmwareMd5 = normalizeMd5Hex(firmware["md5"] | "");
  _firmwareSha256 = firmware["sha256"] | "";

  const char* littleFsUrl = littlefs["url"] | "";
  _littleFsUrl = normalizeHttpUrl(littleFsUrl != nullptr ? littleFsUrl : "");
  _littleFsSize = littlefs["size"] | 0;
  _littleFsMd5 = normalizeMd5Hex(littlefs["md5"] | "");
  _littleFsSha256 = littlefs["sha256"] | "";
  _availableLittleFsVersion = littlefs["version"] | "";

  if (_littleFsUrl.length() > 0 && _availableLittleFsVersion.length() == 0) {
    setError("Manifest littlefs.version is required when littlefs.url is set");
    return false;
  }
  return true;
}

String OtaService::normalizeManifestUrl(String url) {
  url = normalizeHttpUrl(url);
  if (url.length() == 0) {
    return url;
  }

  if (url.endsWith("/")) {
    return url + "manifest.json";
  }

  const int slash = url.lastIndexOf('/');
  const String lastSegment = slash >= 0 ? url.substring(slash + 1) : url;
  if (lastSegment.length() == 0) {
    return url + "manifest.json";
  }
  if (lastSegment.indexOf(".json") >= 0) {
    return url;
  }
  if (lastSegment.indexOf('.') < 0) {
    return url + "/manifest.json";
  }
  return url;
}

String OtaService::normalizeHttpUrl(String url) {
  url.trim();
  if (url.length() == 0) {
    return url;
  }
  if (!url.startsWith("http://") && !url.startsWith("https://")) {
    url = String("https://") + url;
  }
  return url;
}

String OtaService::normalizeMd5Hex(String value) {
  value.trim();
  String out;
  out.reserve(value.length());
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
      out += c;
    } else if (c >= 'A' && c <= 'F') {
      out += static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

bool OtaService::isVersionNewer(const String& candidate, const String& current) {
  if (candidate.length() == 0) {
    return false;
  }
  if (current.length() == 0 || current == "unknown") {
    return true;
  }

  size_t cPos = 0;
  size_t curPos = 0;
  for (int i = 0; i < 4; i++) {
    const int cVal = nextVersionToken(candidate, cPos);
    const int curVal = nextVersionToken(current, curPos);
    if (cVal > curVal) return true;
    if (cVal < curVal) return false;
  }
  return false;
}

int OtaService::nextVersionToken(const String& value, size_t& pos) {
  const size_t len = value.length();
  while (pos < len && (value[pos] < '0' || value[pos] > '9')) {
    pos++;
  }
  int out = 0;
  while (pos < len && value[pos] >= '0' && value[pos] <= '9') {
    out = (out * 10) + static_cast<int>(value[pos] - '0');
    pos++;
  }
  return out;
}

bool OtaService::backupSettingsForLittleFsUpdate() {
  if (!LittleFS.exists(kSettingsPath)) {
    clearPendingSettingsBackupFlag();
    return true;
  }

  File file = LittleFS.open(kSettingsPath, "r");
  if (!file) {
    return false;
  }

  const size_t length = file.size();
  if (length == 0) {
    file.close();
    clearPendingSettingsBackupFlag();
    return true;
  }

  std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[length]);
  if (!data) {
    file.close();
    return false;
  }

  const size_t read = file.read(data.get(), length);
  file.close();
  if (read != length) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(kSettingsBackupNamespace, false)) {
    return false;
  }

  const size_t bytesWritten = prefs.putBytes(kBackupBlobKey, data.get(), length);
  const size_t lenWritten = prefs.putUInt(kBackupLengthKey, static_cast<uint32_t>(length));
  const size_t pendingWritten = prefs.putBool(kBackupPendingKey, true);
  prefs.end();
  return bytesWritten == length && lenWritten > 0 && pendingWritten > 0;
}

void OtaService::persistCurrentLittleFsVersion(const String& version) {
  Preferences prefs;
  if (!prefs.begin(kOtaMetaNamespace, false)) {
    return;
  }
  if (version.length() == 0) {
    prefs.remove(kLittleFsVersionKey);
  } else {
    prefs.putString(kLittleFsVersionKey, version);
  }
  prefs.end();
}

void OtaService::clearPendingSettingsBackupFlag() {
  Preferences prefs;
  if (!prefs.begin(kSettingsBackupNamespace, false)) {
    return;
  }
  prefs.remove(kBackupBlobKey);
  prefs.remove(kBackupLengthKey);
  prefs.putBool(kBackupPendingKey, false);
  prefs.end();
}

bool OtaService::loadCurrentLittleFsVersion() {
  Preferences prefs;
  if (!prefs.begin(kOtaMetaNamespace, true)) {
    _currentLittleFsVersion = "";
    return false;
  }
  _currentLittleFsVersion = prefs.getString(kLittleFsVersionKey, "");
  prefs.end();
  return true;
}

void OtaService::setError(const String& message) {
  _lastError = message;
  _state = OtaState::Error;
}

void OtaService::clearError() { _lastError = ""; }
