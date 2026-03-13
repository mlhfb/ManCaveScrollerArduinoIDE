#include "SettingsStore.h"

#include <memory>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "AppConfig.h"

namespace {
const char* kSettingsPath = "/config/settings.json";
const char* kDefaultMessagesPath = "/config/default_messages.json";
const char* kSettingsBackupNamespace = "ota_settings";
const char* kBackupPendingKey = "pending";
const char* kBackupLengthKey = "len";
const char* kBackupBlobKey = "blob";

void safeCopy(char* dst, size_t dstLen, const char* src) {
  if (dst == nullptr || dstLen == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strlcpy(dst, src, dstLen);
}

void clearSettingsBackup() {
  Preferences prefs;
  if (!prefs.begin(kSettingsBackupNamespace, false)) {
    return;
  }
  prefs.remove(kBackupBlobKey);
  prefs.remove(kBackupLengthKey);
  prefs.putBool(kBackupPendingKey, false);
  prefs.end();
}

bool restoreSettingsFromBackupIfPending() {
  Preferences prefs;
  if (!prefs.begin(kSettingsBackupNamespace, true)) {
    return false;
  }

  const bool pending = prefs.getBool(kBackupPendingKey, false);
  if (!pending) {
    prefs.end();
    return false;
  }

  const uint32_t expectedLen = prefs.getUInt(kBackupLengthKey, 0);
  const size_t blobLen = prefs.getBytesLength(kBackupBlobKey);
  if (expectedLen == 0 || blobLen != expectedLen) {
    prefs.end();
    clearSettingsBackup();
    return false;
  }

  std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[blobLen]);
  if (!data) {
    prefs.end();
    return false;
  }

  const size_t readLen = prefs.getBytes(kBackupBlobKey, data.get(), blobLen);
  prefs.end();
  if (readLen != blobLen) {
    clearSettingsBackup();
    return false;
  }

  LittleFS.mkdir("/config");
  File file = LittleFS.open(kSettingsPath, "w");
  if (!file) {
    return false;
  }

  const size_t written = file.write(data.get(), blobLen);
  file.close();
  if (written != blobLen) {
    return false;
  }

  clearSettingsBackup();
  return true;
}
}  // namespace

bool SettingsStore::begin() {
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    return false;
  }

  restoreSettingsFromBackupIfPending();

  loadDefaults();
  if (!load()) {
    save();
  }
  return true;
}

AppSettings& SettingsStore::mutableSettings() { return _settings; }

const AppSettings& SettingsStore::settings() const { return _settings; }

void SettingsStore::loadDefaults() {
  memset(&_settings, 0, sizeof(_settings));
  _settings.schemaVersion = APP_SETTINGS_SCHEMA_VERSION;

  for (size_t i = 0; i < APP_MAX_MESSAGES; i++) {
    _settings.messages[i].r = 255;
    _settings.messages[i].g = 255;
    _settings.messages[i].b = 255;
    _settings.messages[i].enabled = false;
    _settings.messages[i].text[0] = '\0';
  }

  _settings.speed = APP_SCROLL_SPEED_DEFAULT;
  _settings.brightness = APP_DEFAULT_BRIGHTNESS;
  _settings.panelCols = 128;

  safeCopy(_settings.wifiSsid, sizeof(_settings.wifiSsid),
           "Anarchists Anonymous");
  safeCopy(_settings.wifiPassword, sizeof(_settings.wifiPassword), "nitsuj373");

  _settings.rssEnabled = true;
  safeCopy(_settings.rssUrl, sizeof(_settings.rssUrl), "https://feeds.npr.org/1001/rss.xml");
  _settings.rssNprEnabled = true;
  _settings.rssRandomEnabled = false;
  _settings.rssSportsEnabled = true;
  safeCopy(_settings.rssSportsBaseUrl, sizeof(_settings.rssSportsBaseUrl),
           "charlie.servebeer.com/");
  _settings.rssSportMlbEnabled = true;
  _settings.rssSportNhlEnabled = true;
  _settings.rssSportNcaafEnabled = true;
  _settings.rssSportNflEnabled = true;
  _settings.rssSportNbaEnabled = true;
  _settings.rssSportBig10Enabled = true;

  safeCopy(_settings.otaManifestUrl, sizeof(_settings.otaManifestUrl),
           "https://charlie.servebeer.com/OTA/manifest.json");

  if (!loadDefaultMessagesFromFile()) {
    safeCopy(_settings.messages[0].text, sizeof(_settings.messages[0].text),
             "Hello Man Cave!");
    _settings.messages[0].r = 255;
    _settings.messages[0].g = 0;
    _settings.messages[0].b = 0;
    _settings.messages[0].enabled = true;
  }

  sanitize();
}

bool SettingsStore::loadDefaultMessagesFromFile() {
  File file = LittleFS.open(kDefaultMessagesPath, "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }

  JsonArray messages = doc["messages"].as<JsonArray>();
  if (messages.isNull()) {
    return false;
  }

  bool anyEnabled = false;
  size_t idx = 0;
  for (JsonObject msg : messages) {
    if (idx >= APP_MAX_MESSAGES) {
      break;
    }

    safeCopy(_settings.messages[idx].text, sizeof(_settings.messages[idx].text),
             msg["text"] | "");
    _settings.messages[idx].r = static_cast<uint8_t>(msg["r"] | 255);
    _settings.messages[idx].g = static_cast<uint8_t>(msg["g"] | 255);
    _settings.messages[idx].b = static_cast<uint8_t>(msg["b"] | 255);
    _settings.messages[idx].enabled = msg["enabled"] | false;
    if (_settings.messages[idx].enabled &&
        _settings.messages[idx].text[0] != '\0') {
      anyEnabled = true;
    }
    idx++;
  }

  return anyEnabled;
}

bool SettingsStore::load() {
  File file = LittleFS.open(kSettingsPath, "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }

  uint16_t schema = doc["schema_version"] | 0;
  if (schema > APP_SETTINGS_SCHEMA_VERSION) {
    return false;
  }

  // Start from defaults, then overlay saved values.
  AppSettings defaults = _settings;

  JsonArray msgs = doc["messages"].as<JsonArray>();
  if (!msgs.isNull()) {
    size_t i = 0;
    for (JsonObject m : msgs) {
      if (i >= APP_MAX_MESSAGES) break;
      safeCopy(_settings.messages[i].text, sizeof(_settings.messages[i].text),
               m["text"] | defaults.messages[i].text);
      _settings.messages[i].r = static_cast<uint8_t>(m["r"] | defaults.messages[i].r);
      _settings.messages[i].g = static_cast<uint8_t>(m["g"] | defaults.messages[i].g);
      _settings.messages[i].b = static_cast<uint8_t>(m["b"] | defaults.messages[i].b);
      _settings.messages[i].enabled = m["enabled"] | defaults.messages[i].enabled;
      i++;
    }
  }

  _settings.speed = static_cast<uint8_t>(doc["speed"] | defaults.speed);
  _settings.brightness =
      static_cast<uint8_t>(doc["brightness"] | defaults.brightness);
  _settings.panelCols = static_cast<uint8_t>(doc["panel_cols"] | defaults.panelCols);

  safeCopy(_settings.wifiSsid, sizeof(_settings.wifiSsid),
           doc["wifi_ssid"] | defaults.wifiSsid);
  safeCopy(_settings.wifiPassword, sizeof(_settings.wifiPassword),
           doc["wifi_password"] | defaults.wifiPassword);

  _settings.rssEnabled = doc["rss_enabled"] | defaults.rssEnabled;
  safeCopy(_settings.rssUrl, sizeof(_settings.rssUrl), doc["rss_url"] | defaults.rssUrl);
  _settings.rssNprEnabled = doc["rss_npr_enabled"] | defaults.rssNprEnabled;
  _settings.rssRandomEnabled = doc["rss_random_enabled"] | defaults.rssRandomEnabled;
  _settings.rssSportsEnabled =
      doc["rss_sports_enabled"] | defaults.rssSportsEnabled;
  safeCopy(_settings.rssSportsBaseUrl, sizeof(_settings.rssSportsBaseUrl),
           doc["rss_sports_base_url"] | defaults.rssSportsBaseUrl);
  _settings.rssSportMlbEnabled =
      doc["rss_sport_mlb_enabled"] | defaults.rssSportMlbEnabled;
  _settings.rssSportNhlEnabled =
      doc["rss_sport_nhl_enabled"] | defaults.rssSportNhlEnabled;
  _settings.rssSportNcaafEnabled =
      doc["rss_sport_ncaaf_enabled"] | defaults.rssSportNcaafEnabled;
  _settings.rssSportNflEnabled =
      doc["rss_sport_nfl_enabled"] | defaults.rssSportNflEnabled;
  _settings.rssSportNbaEnabled =
      doc["rss_sport_nba_enabled"] | defaults.rssSportNbaEnabled;
  _settings.rssSportBig10Enabled =
      doc["rss_sport_big10_enabled"] | defaults.rssSportBig10Enabled;
  safeCopy(_settings.otaManifestUrl, sizeof(_settings.otaManifestUrl),
           doc["ota_manifest_url"] | defaults.otaManifestUrl);

  _settings.schemaVersion = APP_SETTINGS_SCHEMA_VERSION;
  sanitize();
  return true;
}

bool SettingsStore::save() const {
  DynamicJsonDocument doc(8192);
  doc["schema_version"] = APP_SETTINGS_SCHEMA_VERSION;

  JsonArray msgs = doc.createNestedArray("messages");
  for (size_t i = 0; i < APP_MAX_MESSAGES; i++) {
    JsonObject m = msgs.createNestedObject();
    m["text"] = _settings.messages[i].text;
    m["r"] = _settings.messages[i].r;
    m["g"] = _settings.messages[i].g;
    m["b"] = _settings.messages[i].b;
    m["enabled"] = _settings.messages[i].enabled;
  }

  doc["speed"] = _settings.speed;
  doc["brightness"] = _settings.brightness;
  doc["panel_cols"] = _settings.panelCols;

  doc["wifi_ssid"] = _settings.wifiSsid;
  doc["wifi_password"] = _settings.wifiPassword;

  doc["rss_enabled"] = _settings.rssEnabled;
  doc["rss_url"] = _settings.rssUrl;
  doc["rss_npr_enabled"] = _settings.rssNprEnabled;
  doc["rss_random_enabled"] = _settings.rssRandomEnabled;
  doc["rss_sports_enabled"] = _settings.rssSportsEnabled;
  doc["rss_sports_base_url"] = _settings.rssSportsBaseUrl;
  doc["rss_sport_mlb_enabled"] = _settings.rssSportMlbEnabled;
  doc["rss_sport_nhl_enabled"] = _settings.rssSportNhlEnabled;
  doc["rss_sport_ncaaf_enabled"] = _settings.rssSportNcaafEnabled;
  doc["rss_sport_nfl_enabled"] = _settings.rssSportNflEnabled;
  doc["rss_sport_nba_enabled"] = _settings.rssSportNbaEnabled;
  doc["rss_sport_big10_enabled"] = _settings.rssSportBig10Enabled;
  doc["ota_manifest_url"] = _settings.otaManifestUrl;

  File file = LittleFS.open(kSettingsPath, "w");
  if (!file) {
    return false;
  }

  bool ok = serializeJsonPretty(doc, file) > 0;
  file.close();
  return ok;
}

bool SettingsStore::factoryReset() {
  if (LittleFS.exists(kSettingsPath) && !LittleFS.remove(kSettingsPath)) {
    return false;
  }
  loadDefaults();
  return save();
}

void SettingsStore::sanitize() {
  if (_settings.speed < APP_SCROLL_SPEED_MIN) {
    _settings.speed = APP_SCROLL_SPEED_MIN;
  } else if (_settings.speed > APP_SCROLL_SPEED_MAX) {
    _settings.speed = APP_SCROLL_SPEED_MAX;
  }

  if (_settings.panelCols != 32 && _settings.panelCols != 64 &&
      _settings.panelCols != 96 && _settings.panelCols != 128) {
    _settings.panelCols = 128;
  }
}
