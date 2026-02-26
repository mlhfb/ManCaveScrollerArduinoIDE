#ifndef RSS_RUNTIME_H
#define RSS_RUNTIME_H

#include <Arduino.h>

#include "AppTypes.h"
#include "RssCache.h"
#include "RssFetcher.h"
#include "SettingsStore.h"
#include "WifiService.h"

class RssRuntime {
public:
  RssRuntime(SettingsStore& settingsStore, WifiService& wifiService);

  bool begin();
  void onSettingsChanged(const AppSettings& settings);
  void setSuspended(bool suspended);
  void tick();
  void forceRefreshSoon();

  bool hasEnabledSources() const;
  bool hasCachedContent() const;
  bool cacheReady() const;

  bool nextSegment(String& outText, uint8_t& outR, uint8_t& outG,
                   uint8_t& outB);

  size_t sourceCount() const;
  const RssSource* sources() const;
  bool sourceMetadata(size_t sourceIndex, RssCacheMetadata& outMetadata) const;

private:
  static constexpr uint32_t kRefreshIntervalMs = 15UL * 60UL * 1000UL;
  static constexpr uint32_t kRefreshRetryMs = 60UL * 1000UL;

  bool shouldRefreshNow() const;
  void scheduleNextRefresh(bool success);
  void rebuildSources(const AppSettings& settings);
  bool refreshCache();
  bool pickNextItem();
  void resetPlayback();
  void colorForSource(size_t sourceIndex, uint8_t& outR, uint8_t& outG,
                      uint8_t& outB) const;

  SettingsStore& _settingsStore;
  WifiService& _wifiService;
  RssFetcher _fetcher;
  RssCache _cache;

  RssSource _sources[APP_MAX_RSS_SOURCES];
  size_t _sourceCount;
  bool _suspended;
  uint32_t _nextRefreshMs;

  bool _cacheReady;
  bool _haveCurrentItem;
  bool _showTitleNext;
  size_t _currentSourceIndex;
  RssItem _currentItem;
  RssItem _fetchItems[APP_MAX_RSS_ITEMS];
};

#endif
