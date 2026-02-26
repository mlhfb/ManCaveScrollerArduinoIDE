#include "RssRuntime.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>
#include <string.h>

#include "RssSources.h"
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

namespace {
struct ColorTriplet {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

constexpr ColorTriplet kSourceColors[] = {
    {255, 255, 255}, {255, 255, 0}, {0, 255, 0},   {255, 0, 0},
    {0, 0, 255},     {0, 255, 255}, {148, 0, 211}, {255, 128, 0},
};

constexpr uint32_t kItemsPerInterstitial = 6;
constexpr uint32_t kClockSyncRetryMs = 60UL * 1000UL;
constexpr uint32_t kClockResyncMs = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRefreshMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;
constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.nist.gov";
constexpr const char* kNtpServer3 = "time.google.com";
constexpr const char* kEasternTz = "EST5EDT,M3.2.0/2,M11.1.0/2";
#ifndef APP_WEATHER_API_URL
#define APP_WEATHER_API_URL ""
#endif
constexpr const char* kWeatherApiUrl = APP_WEATHER_API_URL;

bool extractXmlAttribute(const String& xml, const char* tagName, const char* attrName,
                         String& out) {
  if (tagName == nullptr || attrName == nullptr) {
    return false;
  }

  String open = String("<") + tagName;
  const int tagStart = xml.indexOf(open);
  if (tagStart < 0) {
    return false;
  }

  const int tagEnd = xml.indexOf('>', tagStart);
  if (tagEnd < 0) {
    return false;
  }

  String tag = xml.substring(tagStart, tagEnd + 1);
  String key = String(attrName) + "=\"";
  const int valueStart = tag.indexOf(key);
  if (valueStart < 0) {
    return false;
  }

  const int start = valueStart + key.length();
  const int end = tag.indexOf('"', start);
  if (end <= start) {
    return false;
  }

  out = tag.substring(start, end);
  out.trim();
  return out.length() > 0;
}

void appendWeatherPart(String& out, const String& prefix, const String& value,
                       const String& suffix) {
  if (value.length() == 0) {
    return;
  }
  out += prefix;
  out += value;
  out += suffix;
}
}  // namespace

RssRuntime::RssRuntime(SettingsStore& settingsStore, WifiService& wifiService)
    : _settingsStore(settingsStore),
      _wifiService(wifiService),
      _fetcher(),
      _cache(),
      _sources{},
      _sourceCount(0),
      _suspended(false),
      _radioControlEnabled(true),
      _nextRefreshMs(0),
      _cacheReady(false),
      _randomEnabled(true),
      _haveCurrentItem(false),
      _showTitleNext(true),
      _itemsSinceInterstitial(0),
      _interstitialCursor(0),
      _clockSynced(false),
      _clockSyncEpoch(0),
      _clockSyncMillis(0),
      _lastClockSyncAttemptMs(0),
      _weatherMessage("Weather unavailable"),
      _weatherReady(false),
      _pendingStartupWeather(true),
      _weatherLastFetchMs(0),
      _lastWeatherFetchAttemptMs(0),
      _currentSourceIndex(0),
      _currentColorIndex(0),
      _colorRotationIndex(0),
      _orderedSourceIndex(0),
      _orderedItemIndex(0),
      _currentItem{},
      _fetchItems{} {}

bool RssRuntime::begin() {
  if (!_cache.begin()) {
    return false;
  }
  rebuildSources(_settingsStore.settings());
  _cacheReady = hasCachedContent();
  _pendingStartupWeather = true;
  _nextRefreshMs = millis() + 2500;
  return true;
}

void RssRuntime::onSettingsChanged(const AppSettings& settings) {
  rebuildSources(settings);
  _cacheReady = hasCachedContent();
  _pendingStartupWeather = true;
  resetPlayback();
  forceRefreshSoon();
}

void RssRuntime::setSuspended(bool suspended) { _suspended = suspended; }

void RssRuntime::setRadioControlEnabled(bool enabled) {
  _radioControlEnabled = enabled;
}

void RssRuntime::tick() {
  if (_suspended || !hasEnabledSources()) {
    return;
  }
  if (!shouldRefreshNow()) {
    return;
  }

  const bool fetchSuccess = refreshCache();
  scheduleNextRefresh(fetchSuccess);
}

void RssRuntime::forceRefreshSoon() { _nextRefreshMs = millis() + 500; }

bool RssRuntime::hasEnabledSources() const { return _sourceCount > 0; }

bool RssRuntime::hasCachedContent() const {
  for (size_t i = 0; i < _sourceCount; i++) {
    if (_cache.hasItems(_sources[i].url)) {
      return true;
    }
  }
  return false;
}

bool RssRuntime::cacheReady() const { return _cacheReady; }

bool RssRuntime::refreshAllNow() {
  const bool success = refreshCache();
  scheduleNextRefresh(success);
  return success;
}

void RssRuntime::queueStartupWeather() {
  _pendingStartupWeather = true;
  _haveCurrentItem = false;
  _showTitleNext = true;
}

bool RssRuntime::nextSegment(String& outText, uint8_t& outR, uint8_t& outG,
                             uint8_t& outB) {
  if (!hasEnabledSources()) {
    return false;
  }

  if (!_haveCurrentItem && _pendingStartupWeather) {
    if (buildWeatherMessage(outText)) {
      outR = 255;
      outG = 195;
      outB = 0;
      _pendingStartupWeather = false;
      return true;
    }
    _pendingStartupWeather = false;
  }

  if (!_haveCurrentItem && _itemsSinceInterstitial >= kItemsPerInterstitial) {
    if (nextInterstitialSegment(outText, outR, outG, outB)) {
      _itemsSinceInterstitial = 0;
      return true;
    }
  }

  if (!_haveCurrentItem && !pickNextItem()) {
    return false;
  }

  colorForSource(_currentColorIndex, outR, outG, outB);
  bool singleSegment = false;
  if (_currentSourceIndex < _sourceCount) {
    String sourceUrl = _sources[_currentSourceIndex].url;
    sourceUrl.toLowerCase();
    singleSegment = sourceUrl.indexOf("sport=") >= 0;
  }

  if (_showTitleNext) {
    outText = (_currentItem.title[0] != '\0') ? _currentItem.title : "(no title)";
    if (singleSegment && _currentItem.description[0] != '\0') {
      outText += "  ";
      outText += _currentItem.description;
      _showTitleNext = true;
      _haveCurrentItem = false;
      markItemDisplayed();
    } else if (_currentItem.description[0] == '\0') {
      // No description available: show title only and advance item.
      _showTitleNext = true;
      _haveCurrentItem = false;
      markItemDisplayed();
    } else {
      _showTitleNext = false;
    }
  } else {
    outText = _currentItem.description;
    _showTitleNext = true;
    _haveCurrentItem = false;
    markItemDisplayed();
  }
  return true;
}

size_t RssRuntime::sourceCount() const { return _sourceCount; }

const RssSource* RssRuntime::sources() const { return _sources; }

bool RssRuntime::sourceMetadata(size_t sourceIndex,
                                RssCacheMetadata& outMetadata) const {
  if (sourceIndex >= _sourceCount) {
    return false;
  }
  return _cache.metadata(_sources[sourceIndex].url, outMetadata);
}

bool RssRuntime::shouldRefreshNow() const {
  return static_cast<int32_t>(millis() - _nextRefreshMs) >= 0;
}

void RssRuntime::scheduleNextRefresh(bool success) {
  _nextRefreshMs = millis() + (success ? kRefreshIntervalMs : kRefreshRetryMs);
}

void RssRuntime::rebuildSources(const AppSettings& settings) {
  _sourceCount = buildRssSources(settings, _sources, APP_MAX_RSS_SOURCES);
  _randomEnabled = settings.rssRandomEnabled;
}

bool RssRuntime::refreshCache() {
  if (_sourceCount == 0) {
    _cacheReady = false;
    return false;
  }

  const AppSettings& settings = _settingsStore.settings();

  // In config mode we keep current radio state stable so web UI stays reachable.
  if (!_radioControlEnabled) {
    if (_wifiService.mode() != WifiRuntimeMode::StaConnected) {
      _cacheReady = hasCachedContent();
      return false;
    }
    trySyncClockFromNtp(false);
    const bool weatherFetched = refreshWeather();

    bool fetchedAny = false;
    for (size_t i = 0; i < _sourceCount; i++) {
      const RssFetchResult result = _fetcher.fetch(
          _sources[i].url, _fetchItems, APP_MAX_RSS_ITEMS, 3, 10000, 750);
      if (!result.success || result.itemCount == 0) {
        continue;
      }
      if (_cache.store(_sources[i].url, _sources[i].name, _fetchItems,
                       result.itemCount)) {
        fetchedAny = true;
      }
    }

    _cacheReady = hasCachedContent();
    if (fetchedAny || weatherFetched) {
      resetPlayback();
    }
    return fetchedAny || weatherFetched;
  }

  if (settings.wifiSsid[0] == '\0') {
    _cacheReady = hasCachedContent();
    return false;
  }

  if (!_wifiService.connectSta(settings.wifiSsid, settings.wifiPassword, 8000, 2)) {
    _wifiService.stopWifi();
    _cacheReady = hasCachedContent();
    return false;
  }
  trySyncClockFromNtp(false);
  const bool weatherFetched = refreshWeather();

  bool fetchedAny = false;

  for (size_t i = 0; i < _sourceCount; i++) {
    const RssFetchResult result =
        _fetcher.fetch(_sources[i].url, _fetchItems, APP_MAX_RSS_ITEMS, 3, 10000,
                       750);
    if (!result.success || result.itemCount == 0) {
      continue;
    }

    if (_cache.store(_sources[i].url, _sources[i].name, _fetchItems,
                     result.itemCount)) {
      fetchedAny = true;
    }
  }

  _wifiService.stopWifi();
  _cacheReady = hasCachedContent();

  if (fetchedAny || weatherFetched) {
    resetPlayback();
  }
  return fetchedAny || weatherFetched;
}

bool RssRuntime::pickNextItem() {
  if (!_randomEnabled) {
    return pickNextItemOrdered();
  }

  bool cycleReset = false;
  size_t sourceIndex = 0;
  RssItem item = {};
  uint8_t flags = RssItemFlagNone;

  if (!_cache.pickRandomItemNoRepeat(_sources, _sourceCount, item, sourceIndex,
                                     cycleReset, &flags)) {
    _cacheReady = false;
    return false;
  }

  _currentItem = item;
  _currentItem.flags = flags;
  _currentSourceIndex = sourceIndex;
  _currentColorIndex = _colorRotationIndex++;
  _haveCurrentItem = true;
  _showTitleNext = true;
  Serial.print("[RSS] Random pick source=");
  Serial.print(_sources[sourceIndex].name);
  Serial.print(" title=");
  Serial.println(_currentItem.title);
  (void)cycleReset;
  return true;
}

bool RssRuntime::refreshSource(size_t sourceIndex) {
  if (sourceIndex >= _sourceCount) {
    return false;
  }

  Serial.print("[RSS] Refresh source: ");
  Serial.print(_sources[sourceIndex].name);
  Serial.print(" -> ");
  Serial.println(_sources[sourceIndex].url);

  const RssFetchResult result = _fetcher.fetch(
      _sources[sourceIndex].url, _fetchItems, APP_MAX_RSS_ITEMS, 3, 10000, 750);
  if (!result.success || result.itemCount == 0) {
    Serial.print("[RSS] Refresh failed: ");
    Serial.println(result.error);
    return false;
  }
  const bool stored = _cache.store(_sources[sourceIndex].url, _sources[sourceIndex].name,
                                   _fetchItems, result.itemCount);
  Serial.print("[RSS] Refresh ");
  Serial.print(stored ? "stored " : "store failed ");
  Serial.print("items=");
  Serial.print(result.itemCount);
  Serial.print(" source=");
  Serial.println(_sources[sourceIndex].name);
  return stored;
}

bool RssRuntime::refreshSourceWithManagedRadio(size_t sourceIndex) {
  if (sourceIndex >= _sourceCount) {
    return false;
  }

  if (!_radioControlEnabled) {
    if (_wifiService.mode() != WifiRuntimeMode::StaConnected) {
      return false;
    }
    return refreshSource(sourceIndex);
  }

  const AppSettings& settings = _settingsStore.settings();
  if (settings.wifiSsid[0] == '\0') {
    return false;
  }

  if (!_wifiService.connectSta(settings.wifiSsid, settings.wifiPassword, 8000, 2)) {
    _wifiService.stopWifi();
    return false;
  }
  trySyncClockFromNtp(false);

  const bool refreshed = refreshSource(sourceIndex);
  _wifiService.stopWifi();
  return refreshed;
}

bool RssRuntime::pickNextItemOrdered() {
  if (_sourceCount == 0) {
    return false;
  }

  size_t sourceAttempts = 0;
  while (sourceAttempts < _sourceCount) {
    if (_orderedSourceIndex >= _sourceCount) {
      _orderedSourceIndex = 0;
    }
    const size_t sourceIndex = _orderedSourceIndex;

    // Mimic rssArduinoPlatform: refresh selected source at start of source cycle
    // before traversing all its items.
    if (_orderedItemIndex == 0) {
      refreshSourceWithManagedRadio(sourceIndex);
      _cacheReady = hasCachedContent();
    }

    uint32_t count = 0;
    if (!_cache.itemCount(_sources[sourceIndex].url, count) || count == 0) {
      _orderedSourceIndex = (_orderedSourceIndex + 1) % _sourceCount;
      _orderedItemIndex = 0;
      sourceAttempts++;
      continue;
    }

    if (_orderedItemIndex >= count) {
      _orderedSourceIndex = (_orderedSourceIndex + 1) % _sourceCount;
      _orderedItemIndex = 0;
      sourceAttempts++;
      continue;
    }

    RssItem item = {};
    if (!_cache.loadItem(_sources[sourceIndex].url, _orderedItemIndex, item)) {
      _orderedSourceIndex = (_orderedSourceIndex + 1) % _sourceCount;
      _orderedItemIndex = 0;
      sourceAttempts++;
      continue;
    }

    _currentItem = item;
    _currentSourceIndex = sourceIndex;
    _currentColorIndex = _colorRotationIndex++;
    _haveCurrentItem = true;
    _showTitleNext = true;

    Serial.print("[RSS] Ordered pick source=");
    Serial.print(_sources[sourceIndex].name);
    Serial.print(" item=");
    Serial.print(_orderedItemIndex + 1);
    Serial.print("/");
    Serial.print(count);
    Serial.print(" title=");
    Serial.println(_currentItem.title);

    _orderedItemIndex++;
    if (_orderedItemIndex >= count) {
      _orderedSourceIndex = (_orderedSourceIndex + 1) % _sourceCount;
      _orderedItemIndex = 0;
    }
    return true;
  }

  _cacheReady = hasCachedContent();
  return false;
}

void RssRuntime::resetPlayback() {
  _haveCurrentItem = false;
  _showTitleNext = true;
  _itemsSinceInterstitial = 0;
  _interstitialCursor = 0;
  _currentSourceIndex = 0;
  _currentColorIndex = 0;
  _orderedSourceIndex = 0;
  _orderedItemIndex = 0;
  memset(&_currentItem, 0, sizeof(_currentItem));
}

bool RssRuntime::nextInterstitialSegment(String& outText, uint8_t& outR,
                                         uint8_t& outG, uint8_t& outB) {
  const AppSettings& settings = _settingsStore.settings();
  constexpr uint8_t kSlotCount = static_cast<uint8_t>(APP_MAX_MESSAGES + 2);
  for (uint8_t attempt = 0; attempt < kSlotCount; attempt++) {
    const uint8_t slot = _interstitialCursor;
    _interstitialCursor = static_cast<uint8_t>((_interstitialCursor + 1) % kSlotCount);

    if (slot == 0) {
      if (buildTimeMessage(outText)) {
        outR = 180;
        outG = 235;
        outB = 255;
        Serial.print("[RSS] Interstitial time: ");
        Serial.println(outText);
        return true;
      }
      continue;
    }

    if (slot == 1) {
      if (buildWeatherMessage(outText)) {
        outR = 255;
        outG = 195;
        outB = 0;
        Serial.print("[RSS] Interstitial weather: ");
        Serial.println(outText);
        return true;
      }
      continue;
    }

    const size_t msgIdx = static_cast<size_t>(slot - 2);
    if (msgIdx >= APP_MAX_MESSAGES) {
      continue;
    }
    const AppMessage& msg = settings.messages[msgIdx];
    if (!msg.enabled || msg.text[0] == '\0') {
      continue;
    }

    outText = msg.text;
    outR = msg.r;
    outG = msg.g;
    outB = msg.b;
    Serial.print("[RSS] Interstitial message ");
    Serial.print(msgIdx + 1);
    Serial.print(": ");
    Serial.println(outText);
    return true;
  }

  if (!buildTimeMessage(outText)) {
    if (!buildWeatherMessage(outText)) {
      outText = "Time unavailable";
    }
  }
  outR = 180;
  outG = 235;
  outB = 255;
  Serial.print("[RSS] Interstitial fallback: ");
  Serial.println(outText);
  return true;
}

bool RssRuntime::buildTimeMessage(String& outText) {
  if (!_clockSynced && _wifiService.mode() == WifiRuntimeMode::StaConnected) {
    trySyncClockFromNtp(true);
  }

  if (_clockSynced) {
    time_t epoch = _clockSyncEpoch;
    if (_clockSyncMillis != 0) {
      const uint32_t elapsedMs = millis() - _clockSyncMillis;
      epoch += static_cast<time_t>(elapsedMs / 1000UL);
    }

    struct tm tmLocal = {};
    localtime_r(&epoch, &tmLocal);

    char buf[40] = {0};
    strftime(buf, sizeof(buf), "%a %b %d -- %H:%M", &tmLocal);
    outText = buf;
    outText.toUpperCase();
    return true;
  }

  outText = "Time unavailable";
  return true;
}

bool RssRuntime::buildWeatherMessage(String& outText) {
  const uint32_t nowMs = millis();
  const bool stale = !_weatherReady ||
                     (_weatherLastFetchMs != 0 &&
                      static_cast<uint32_t>(nowMs - _weatherLastFetchMs) >=
                          kWeatherRefreshMs);
  const bool retryAllowed =
      (_lastWeatherFetchAttemptMs == 0) ||
      (static_cast<uint32_t>(nowMs - _lastWeatherFetchAttemptMs) >= kWeatherRetryMs);

  if (stale && retryAllowed) {
    refreshWeatherWithManagedRadio();
  }

  outText = _weatherReady ? _weatherMessage : String("Weather unavailable");
  return true;
}

bool RssRuntime::refreshWeather() {
  if (_wifiService.mode() != WifiRuntimeMode::StaConnected) {
    return false;
  }
  if (kWeatherApiUrl[0] == '\0') {
    Serial.println("[WEATHER] APP_WEATHER_API_URL not configured");
    return false;
  }

  _lastWeatherFetchAttemptMs = millis();
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, kWeatherApiUrl)) {
    Serial.println("[WEATHER] HTTP begin failed");
    return false;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.print("[WEATHER] HTTP status ");
    Serial.println(status);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  if (payload.length() == 0) {
    Serial.println("[WEATHER] Empty payload");
    return false;
  }

  String city;
  String temp;
  String humidity;
  String pressure;
  String windSpeed;
  String windDirection;
  String clouds;
  extractXmlAttribute(payload, "city", "name", city);
  extractXmlAttribute(payload, "temperature", "value", temp);
  extractXmlAttribute(payload, "humidity", "value", humidity);
  extractXmlAttribute(payload, "pressure", "value", pressure);
  extractXmlAttribute(payload, "speed", "value", windSpeed);
  extractXmlAttribute(payload, "direction", "name", windDirection);
  extractXmlAttribute(payload, "clouds", "name", clouds);

  String message;
  appendWeatherPart(message, "Weather for ", city, " Michigan ...The Prison City... ");
  appendWeatherPart(message, "Current Tempurature: ", temp, "F ");
  appendWeatherPart(message, "Humidity ", humidity, "% ");
  appendWeatherPart(message, "Pressure ", pressure, " kpa ");
  appendWeatherPart(message, "Wind ", windSpeed, " mph");
  appendWeatherPart(message, " from the ", windDirection, "  ");
  appendWeatherPart(message, " .... ", clouds, "....");

  message.trim();
  if (message.length() == 0) {
    Serial.println("[WEATHER] Parse failed");
    return false;
  }

  _weatherMessage = message;
  _weatherReady = true;
  _weatherLastFetchMs = millis();
  Serial.print("[WEATHER] Updated: ");
  Serial.println(_weatherMessage);
  return true;
}

bool RssRuntime::refreshWeatherWithManagedRadio() {
  if (!_radioControlEnabled) {
    if (_wifiService.mode() != WifiRuntimeMode::StaConnected) {
      return false;
    }
    return refreshWeather();
  }

  const AppSettings& settings = _settingsStore.settings();
  if (settings.wifiSsid[0] == '\0') {
    return false;
  }

  if (!_wifiService.connectSta(settings.wifiSsid, settings.wifiPassword, 8000, 2)) {
    _wifiService.stopWifi();
    return false;
  }
  trySyncClockFromNtp(false);
  const bool refreshed = refreshWeather();
  _wifiService.stopWifi();
  return refreshed;
}

void RssRuntime::markItemDisplayed() { _itemsSinceInterstitial++; }

void RssRuntime::trySyncClockFromNtp(bool force) {
  if (_wifiService.mode() != WifiRuntimeMode::StaConnected) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force) {
    if (_lastClockSyncAttemptMs != 0 &&
        static_cast<uint32_t>(nowMs - _lastClockSyncAttemptMs) < kClockSyncRetryMs) {
      return;
    }
    if (_clockSynced &&
        static_cast<uint32_t>(nowMs - _clockSyncMillis) < kClockResyncMs) {
      return;
    }
  }

  _lastClockSyncAttemptMs = nowMs;
  configTzTime(kEasternTz, kNtpServer1, kNtpServer2, kNtpServer3);

  struct tm tmInfo = {};
  if (!getLocalTime(&tmInfo, 2500)) {
    Serial.println("[NTP] Sync failed");
    return;
  }

  const time_t nowEpoch = time(nullptr);
  if (nowEpoch <= 0) {
    Serial.println("[NTP] Sync failed: invalid epoch");
    return;
  }

  _clockSynced = true;
  _clockSyncEpoch = nowEpoch;
  _clockSyncMillis = millis();
  Serial.print("[NTP] Sync ok epoch=");
  Serial.println(static_cast<long>(_clockSyncEpoch));
}

void RssRuntime::colorForSource(size_t sourceIndex, uint8_t& outR, uint8_t& outG,
                                uint8_t& outB) const {
  const ColorTriplet& c = kSourceColors[sourceIndex % (sizeof(kSourceColors) /
                                                       sizeof(kSourceColors[0]))];
  outR = c.r;
  outG = c.g;
  outB = c.b;
}
