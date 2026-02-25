#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <Arduino.h>

constexpr size_t APP_MAX_MESSAGES = 5;
constexpr size_t APP_MAX_TEXT_LEN = 200;
constexpr size_t APP_MAX_SSID_LEN = 32;
constexpr size_t APP_MAX_PASS_LEN = 64;
constexpr size_t APP_MAX_URL_LEN = 256;
constexpr size_t APP_MAX_RSS_ITEMS = 30;
constexpr size_t APP_MAX_RSS_SOURCES = 8;
constexpr size_t APP_RSS_SOURCE_NAME_LEN = 24;
constexpr size_t APP_RSS_TITLE_LEN = 200;
constexpr size_t APP_RSS_DESC_LEN = 200;
constexpr uint16_t APP_SETTINGS_SCHEMA_VERSION = 1;

struct AppMessage {
  char text[APP_MAX_TEXT_LEN + 1];
  uint8_t r;
  uint8_t g;
  uint8_t b;
  bool enabled;
};

struct AppSettings {
  uint16_t schemaVersion;
  AppMessage messages[APP_MAX_MESSAGES];

  uint8_t speed;
  uint8_t brightness;
  uint8_t panelCols;

  char wifiSsid[APP_MAX_SSID_LEN + 1];
  char wifiPassword[APP_MAX_PASS_LEN + 1];

  bool rssEnabled;
  char rssUrl[APP_MAX_URL_LEN + 1];
  bool rssNprEnabled;
  bool rssSportsEnabled;
  char rssSportsBaseUrl[APP_MAX_URL_LEN + 1];
  bool rssSportMlbEnabled;
  bool rssSportNhlEnabled;
  bool rssSportNcaafEnabled;
  bool rssSportNflEnabled;
  bool rssSportNbaEnabled;
  bool rssSportBig10Enabled;
};

struct RssSource {
  char name[APP_RSS_SOURCE_NAME_LEN + 1];
  char url[APP_MAX_URL_LEN + 1];
  bool enabled;
};

struct RssItem {
  char title[APP_RSS_TITLE_LEN + 1];
  char description[APP_RSS_DESC_LEN + 1];
  uint8_t flags;
};

enum RssItemFlags : uint8_t {
  RssItemFlagNone = 0,
  RssItemFlagLive = 0x01,
};

#endif
