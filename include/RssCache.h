#ifndef RSS_CACHE_H
#define RSS_CACHE_H

#include <Arduino.h>

#include "AppTypes.h"

struct RssCacheMetadata {
  bool valid;
  uint32_t itemCount;
  uint32_t updatedEpoch;
};

class RssCache {
public:
  RssCache();
  ~RssCache();

  bool begin();
  bool store(const char* sourceUrl, const char* sourceName, const RssItem* items,
             size_t itemCount);
  bool hasItems(const char* sourceUrl) const;
  bool metadata(const char* sourceUrl, RssCacheMetadata& outMetadata) const;

  bool pickRandomItemNoRepeat(const RssSource* sources, size_t sourceCount,
                              RssItem& outItem, size_t& outSourceIndex,
                              bool& outCycleReset, uint8_t* outFlags = nullptr);

private:
  struct CacheHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t itemCount;
    uint32_t updatedEpoch;
  };

  struct CacheRecord {
    char title[APP_RSS_TITLE_LEN + 1];
    char description[APP_RSS_DESC_LEN + 1];
    uint8_t flags;
  };

  struct CycleSourceState {
    uint32_t itemCount;
    uint32_t shownCount;
    uint8_t* shownBits;
  };

  void invalidateCycleState();
  void freeCycleState();
  bool ensureCycleState(const RssSource* sources, size_t sourceCount);
  void restartCycleState();

  bool readHeader(const char* sourceUrl, CacheHeader& outHeader) const;
  bool readRecord(const char* sourceUrl, uint32_t itemIndex,
                  CacheRecord& outRecord) const;
  static uint8_t inferItemFlags(const RssItem& item);
  static bool containsCaseInsensitive(const char* haystack, const char* needle);

  bool _cycleValid;
  uint32_t _cycleSignature;
  size_t _cycleSourceCount;
  uint32_t _cycleTotalItems;
  uint32_t _cycleRemainingItems;
  CycleSourceState _cycleSources[APP_MAX_RSS_SOURCES];
};

#endif
