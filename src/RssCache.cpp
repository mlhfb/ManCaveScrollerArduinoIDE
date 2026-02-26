#include "RssCache.h"

#include <LittleFS.h>
#include <ctype.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {
constexpr char kCacheDir[] = "/cache";
constexpr uint32_t kCacheMagic = 0x52434348u;  // "RCCH"
constexpr uint16_t kCacheVersion = 1u;

uint32_t fnv1a(const char* value) {
  uint32_t hash = 2166136261u;
  if (value == nullptr) {
    return hash;
  }
  while (*value) {
    hash ^= static_cast<uint8_t>(*value++);
    hash *= 16777619u;
  }
  return hash;
}

uint32_t hashMixU32(uint32_t hash, uint32_t value) {
  hash ^= (value & 0xFFu);
  hash *= 16777619u;
  hash ^= ((value >> 8) & 0xFFu);
  hash *= 16777619u;
  hash ^= ((value >> 16) & 0xFFu);
  hash *= 16777619u;
  hash ^= ((value >> 24) & 0xFFu);
  hash *= 16777619u;
  return hash;
}

void buildCachePath(const char* sourceUrl, char* outPath, size_t outPathLen) {
  if (outPath == nullptr || outPathLen == 0) {
    return;
  }
  snprintf(outPath, outPathLen, "%s/%08lx.bin", kCacheDir,
           static_cast<unsigned long>(fnv1a(sourceUrl)));
}

inline bool bitGetLocal(const uint8_t* bits, uint32_t idx) {
  return bits != nullptr && (bits[idx / 8] & (1u << (idx % 8))) != 0;
}

inline void bitSetLocal(uint8_t* bits, uint32_t idx) {
  if (bits != nullptr) {
    bits[idx / 8] |= (1u << (idx % 8));
  }
}
}  // namespace

RssCache::RssCache()
    : _cycleValid(false),
      _cycleSignature(0),
      _cycleSourceCount(0),
      _cycleTotalItems(0),
      _cycleRemainingItems(0),
      _cycleSources{} {}

RssCache::~RssCache() { freeCycleState(); }

bool RssCache::begin() {
  if (!LittleFS.exists(kCacheDir) && !LittleFS.mkdir(kCacheDir)) {
    return false;
  }
  freeCycleState();
  return true;
}

bool RssCache::store(const char* sourceUrl, const char* /*sourceName*/,
                     const RssItem* items, size_t itemCount) {
  if (sourceUrl == nullptr || sourceUrl[0] == '\0' || items == nullptr ||
      itemCount == 0) {
    return false;
  }

  char finalPath[64] = {0};
  buildCachePath(sourceUrl, finalPath, sizeof(finalPath));
  char tempPath[80] = {0};
  snprintf(tempPath, sizeof(tempPath), "%s.tmp", finalPath);

  File out = LittleFS.open(tempPath, "w");
  if (!out) {
    return false;
  }

  const time_t now = time(nullptr);
  const uint32_t epoch = (now > 0) ? static_cast<uint32_t>(now) : millis() / 1000;

  CacheHeader header = {};
  header.magic = kCacheMagic;
  header.version = kCacheVersion;
  header.itemCount = static_cast<uint32_t>(itemCount);
  header.updatedEpoch = epoch;

  if (out.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) !=
      sizeof(header)) {
    out.close();
    LittleFS.remove(tempPath);
    return false;
  }

  for (size_t i = 0; i < itemCount; i++) {
    CacheRecord record = {};
    strlcpy(record.title, items[i].title, sizeof(record.title));
    strlcpy(record.description, items[i].description, sizeof(record.description));
    record.flags = items[i].flags | inferItemFlags(items[i]);

    if (out.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record)) !=
        sizeof(record)) {
      out.close();
      LittleFS.remove(tempPath);
      return false;
    }
  }

  out.close();

  if (LittleFS.exists(finalPath)) {
    LittleFS.remove(finalPath);
  }
  if (!LittleFS.rename(tempPath, finalPath)) {
    LittleFS.remove(tempPath);
    return false;
  }

  invalidateCycleState();
  return true;
}

bool RssCache::hasItems(const char* sourceUrl) const {
  CacheHeader header = {};
  if (!readHeader(sourceUrl, header)) {
    return false;
  }
  return header.itemCount > 0;
}

bool RssCache::metadata(const char* sourceUrl,
                        RssCacheMetadata& outMetadata) const {
  outMetadata.valid = false;
  outMetadata.itemCount = 0;
  outMetadata.updatedEpoch = 0;

  CacheHeader header = {};
  if (!readHeader(sourceUrl, header)) {
    return false;
  }

  outMetadata.valid = true;
  outMetadata.itemCount = header.itemCount;
  outMetadata.updatedEpoch = header.updatedEpoch;
  return true;
}

bool RssCache::itemCount(const char* sourceUrl, uint32_t& outCount) const {
  outCount = 0;
  CacheHeader header = {};
  if (!readHeader(sourceUrl, header)) {
    return false;
  }
  outCount = header.itemCount;
  return true;
}

bool RssCache::loadItem(const char* sourceUrl, uint32_t itemIndex,
                        RssItem& outItem) const {
  CacheRecord record = {};
  if (!readRecord(sourceUrl, itemIndex, record)) {
    return false;
  }

  memset(&outItem, 0, sizeof(outItem));
  strlcpy(outItem.title, record.title, sizeof(outItem.title));
  strlcpy(outItem.description, record.description, sizeof(outItem.description));
  outItem.flags = record.flags;
  if (outItem.flags == RssItemFlagNone) {
    outItem.flags = inferItemFlags(outItem);
  }
  return true;
}

bool RssCache::pickRandomItemNoRepeat(const RssSource* sources, size_t sourceCount,
                                      RssItem& outItem, size_t& outSourceIndex,
                                      bool& outCycleReset, uint8_t* outFlags) {
  outCycleReset = false;
  if (outFlags != nullptr) {
    *outFlags = RssItemFlagNone;
  }

  if (sources == nullptr || sourceCount == 0) {
    return false;
  }
  if (sourceCount > APP_MAX_RSS_SOURCES) {
    sourceCount = APP_MAX_RSS_SOURCES;
  }

  if (!ensureCycleState(sources, sourceCount) || _cycleTotalItems == 0) {
    return false;
  }

  if (_cycleRemainingItems == 0) {
    restartCycleState();
    outCycleReset = true;
  }

  uint32_t pick = esp_random() % _cycleRemainingItems;
  int selectedSource = -1;
  uint32_t selectedUnshownRank = 0;

  for (size_t i = 0; i < _cycleSourceCount; i++) {
    CycleSourceState& state = _cycleSources[i];
    if (state.itemCount == 0 || state.shownCount >= state.itemCount) {
      continue;
    }
    const uint32_t remaining = state.itemCount - state.shownCount;
    if (pick < remaining) {
      selectedSource = static_cast<int>(i);
      selectedUnshownRank = pick;
      break;
    }
    pick -= remaining;
  }

  if (selectedSource < 0) {
    return false;
  }

  CycleSourceState& sourceState = _cycleSources[selectedSource];
  uint32_t selectedItemIdx = UINT32_MAX;
  uint32_t rank = 0;

  for (uint32_t i = 0; i < sourceState.itemCount; i++) {
    if (bitGetLocal(sourceState.shownBits, i)) {
      continue;
    }
    if (rank == selectedUnshownRank) {
      selectedItemIdx = i;
      break;
    }
    rank++;
  }

  if (selectedItemIdx == UINT32_MAX) {
    return false;
  }

  CacheRecord record = {};
  if (!readRecord(sources[selectedSource].url, selectedItemIdx, record)) {
    return false;
  }

  bitSetLocal(sourceState.shownBits, selectedItemIdx);
  sourceState.shownCount++;
  _cycleRemainingItems--;

  memset(&outItem, 0, sizeof(outItem));
  strlcpy(outItem.title, record.title, sizeof(outItem.title));
  strlcpy(outItem.description, record.description, sizeof(outItem.description));
  outItem.flags = record.flags;
  if (outItem.flags == RssItemFlagNone) {
    outItem.flags = inferItemFlags(outItem);
  }

  outSourceIndex = static_cast<size_t>(selectedSource);
  if (outFlags != nullptr) {
    *outFlags = outItem.flags;
  }

  return true;
}

void RssCache::invalidateCycleState() { _cycleValid = false; }

void RssCache::freeCycleState() {
  for (size_t i = 0; i < APP_MAX_RSS_SOURCES; i++) {
    free(_cycleSources[i].shownBits);
    _cycleSources[i].shownBits = nullptr;
    _cycleSources[i].itemCount = 0;
    _cycleSources[i].shownCount = 0;
  }
  _cycleValid = false;
  _cycleSignature = 0;
  _cycleSourceCount = 0;
  _cycleTotalItems = 0;
  _cycleRemainingItems = 0;
}

bool RssCache::ensureCycleState(const RssSource* sources, size_t sourceCount) {
  CacheHeader headers[APP_MAX_RSS_SOURCES] = {};
  bool hasHeader[APP_MAX_RSS_SOURCES] = {};

  for (size_t i = 0; i < sourceCount; i++) {
    hasHeader[i] = readHeader(sources[i].url, headers[i]);
  }

  uint32_t signature = 2166136261u;
  signature = hashMixU32(signature, static_cast<uint32_t>(sourceCount));
  for (size_t i = 0; i < sourceCount; i++) {
    signature = hashMixU32(signature, fnv1a(sources[i].url));
    signature = hashMixU32(signature, hasHeader[i] ? headers[i].itemCount : 0u);
    signature = hashMixU32(signature,
                           hasHeader[i] ? headers[i].updatedEpoch : 0u);
  }

  if (_cycleValid && _cycleSignature == signature &&
      _cycleSourceCount == sourceCount) {
    return true;
  }

  freeCycleState();
  _cycleSignature = signature;
  _cycleSourceCount = sourceCount;
  _cycleTotalItems = 0;

  for (size_t i = 0; i < sourceCount; i++) {
    if (!hasHeader[i] || headers[i].itemCount == 0) {
      continue;
    }

    CycleSourceState& state = _cycleSources[i];
    state.itemCount = headers[i].itemCount;
    state.shownCount = 0;

    const size_t bitBytes = (state.itemCount + 7u) / 8u;
    state.shownBits = reinterpret_cast<uint8_t*>(calloc(1, bitBytes));
    if (state.shownBits == nullptr) {
      freeCycleState();
      return false;
    }

    _cycleTotalItems += state.itemCount;
  }

  _cycleRemainingItems = _cycleTotalItems;
  _cycleValid = true;
  return true;
}

void RssCache::restartCycleState() {
  for (size_t i = 0; i < _cycleSourceCount; i++) {
    CycleSourceState& state = _cycleSources[i];
    if (state.shownBits == nullptr || state.itemCount == 0) {
      continue;
    }
    const size_t bitBytes = (state.itemCount + 7u) / 8u;
    memset(state.shownBits, 0, bitBytes);
    state.shownCount = 0;
  }
  _cycleRemainingItems = _cycleTotalItems;
}

bool RssCache::readHeader(const char* sourceUrl, CacheHeader& outHeader) const {
  if (sourceUrl == nullptr || sourceUrl[0] == '\0') {
    return false;
  }

  char path[64] = {0};
  buildCachePath(sourceUrl, path, sizeof(path));
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  const size_t readLen =
      file.readBytes(reinterpret_cast<char*>(&outHeader), sizeof(outHeader));
  file.close();

  if (readLen != sizeof(outHeader)) {
    return false;
  }
  if (outHeader.magic != kCacheMagic || outHeader.version != kCacheVersion) {
    return false;
  }
  return true;
}

bool RssCache::readRecord(const char* sourceUrl, uint32_t itemIndex,
                          CacheRecord& outRecord) const {
  CacheHeader header = {};
  if (!readHeader(sourceUrl, header) || itemIndex >= header.itemCount) {
    return false;
  }

  char path[64] = {0};
  buildCachePath(sourceUrl, path, sizeof(path));
  if (!LittleFS.exists(path)) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  const uint32_t offset =
      sizeof(CacheHeader) + (itemIndex * sizeof(CacheRecord));
  if (!file.seek(offset, SeekSet)) {
    file.close();
    return false;
  }

  const size_t readLen =
      file.readBytes(reinterpret_cast<char*>(&outRecord), sizeof(outRecord));
  file.close();
  return readLen == sizeof(outRecord);
}

uint8_t RssCache::inferItemFlags(const RssItem& item) {
  const char* finishedMarkers[] = {" final",   "final ",  "final/",
                                   "postponed", "cancelled", "canceled",
                                   "suspended"};

  for (const char* marker : finishedMarkers) {
    if (containsCaseInsensitive(item.title, marker) ||
        containsCaseInsensitive(item.description, marker)) {
      return RssItemFlagNone;
    }
  }

  const char* liveMarkers[] = {"in progress", "halftime", "top ",      "bottom ",
                               "bot ",        "end of ",  "start of ", "q1",
                               "q2",          "q3",       "q4",        "1st period",
                               "2nd period",  "3rd period", "overtime", " ot "};

  for (const char* marker : liveMarkers) {
    if (containsCaseInsensitive(item.title, marker) ||
        containsCaseInsensitive(item.description, marker)) {
      return RssItemFlagLive;
    }
  }

  return RssItemFlagNone;
}

bool RssCache::containsCaseInsensitive(const char* haystack,
                                       const char* needle) {
  if (haystack == nullptr || needle == nullptr || needle[0] == '\0') {
    return false;
  }

  const size_t needleLen = strlen(needle);
  for (const char* h = haystack; *h != '\0'; h++) {
    size_t i = 0;
    while (i < needleLen && h[i] != '\0' &&
           tolower(static_cast<unsigned char>(h[i])) ==
               tolower(static_cast<unsigned char>(needle[i]))) {
      i++;
    }
    if (i == needleLen) {
      return true;
    }
  }
  return false;
}
