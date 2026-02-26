#ifndef RSS_FETCHER_H
#define RSS_FETCHER_H

#include <Arduino.h>

#include "AppTypes.h"

struct RssFetchResult {
  bool success;
  uint16_t itemCount;
  int httpStatus;
  String error;
};

class RssFetcher {
public:
  RssFetcher();

  RssFetchResult fetch(const char* url, RssItem* outItems, size_t maxItems,
                       uint8_t maxAttempts = 3, uint32_t timeoutMs = 10000,
                       uint32_t backoffMs = 800) const;

private:
  uint16_t parseRssXml(const String& xml, RssItem* outItems,
                       size_t maxItems) const;
  uint16_t parseJsonFeed(const String& payload, RssItem* outItems,
                         size_t maxItems) const;
};

#endif
