#include "RssRuntime.h"

#include <string.h>

#include "RssSources.h"

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
}  // namespace

RssRuntime::RssRuntime(SettingsStore& settingsStore, WifiService& wifiService)
    : _settingsStore(settingsStore),
      _wifiService(wifiService),
      _fetcher(),
      _cache(),
      _sources{},
      _sourceCount(0),
      _suspended(false),
      _nextRefreshMs(0),
      _cacheReady(false),
      _haveCurrentItem(false),
      _showTitleNext(true),
      _currentSourceIndex(0),
      _currentItem{},
      _fetchItems{} {}

bool RssRuntime::begin() {
  if (!_cache.begin()) {
    return false;
  }
  rebuildSources(_settingsStore.settings());
  _cacheReady = hasCachedContent();
  _nextRefreshMs = millis() + 2500;
  return true;
}

void RssRuntime::onSettingsChanged(const AppSettings& settings) {
  rebuildSources(settings);
  _cacheReady = hasCachedContent();
  resetPlayback();
  forceRefreshSoon();
}

void RssRuntime::setSuspended(bool suspended) { _suspended = suspended; }

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

bool RssRuntime::nextSegment(String& outText, uint8_t& outR, uint8_t& outG,
                             uint8_t& outB) {
  if (!hasEnabledSources() || !_cacheReady) {
    return false;
  }

  if (!_haveCurrentItem && !pickNextItem()) {
    return false;
  }

  colorForSource(_currentSourceIndex, outR, outG, outB);
  if (_showTitleNext) {
    outText = (_currentItem.title[0] != '\0') ? _currentItem.title : "(no title)";
    _showTitleNext = false;
  } else {
    outText = (_currentItem.description[0] != '\0') ? _currentItem.description
                                                    : "(no description)";
    _showTitleNext = true;
    _haveCurrentItem = false;
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
}

bool RssRuntime::refreshCache() {
  if (_sourceCount == 0) {
    _cacheReady = false;
    return false;
  }

  const AppSettings& settings = _settingsStore.settings();
  if (settings.wifiSsid[0] == '\0') {
    _cacheReady = hasCachedContent();
    return false;
  }

  if (!_wifiService.connectSta(settings.wifiSsid, settings.wifiPassword, 8000, 2)) {
    _wifiService.stopWifi();
    _cacheReady = hasCachedContent();
    return false;
  }

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

  if (fetchedAny) {
    resetPlayback();
  }
  return fetchedAny;
}

bool RssRuntime::pickNextItem() {
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
  _haveCurrentItem = true;
  _showTitleNext = true;
  (void)cycleReset;
  return true;
}

void RssRuntime::resetPlayback() {
  _haveCurrentItem = false;
  _showTitleNext = true;
  _currentSourceIndex = 0;
  memset(&_currentItem, 0, sizeof(_currentItem));
}

void RssRuntime::colorForSource(size_t sourceIndex, uint8_t& outR, uint8_t& outG,
                                uint8_t& outB) const {
  const ColorTriplet& c = kSourceColors[sourceIndex % (sizeof(kSourceColors) /
                                                       sizeof(kSourceColors[0]))];
  outR = c.r;
  outG = c.g;
  outB = c.b;
}
