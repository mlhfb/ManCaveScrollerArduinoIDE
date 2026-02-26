#include "RssSources.h"

#include <ctype.h>
#include <string.h>

namespace {
struct SportDef {
  const char* key;
  const char* label;
  bool AppSettings::*enabledField;
};

constexpr SportDef kSports[] = {
    {"mlb", "MLB", &AppSettings::rssSportMlbEnabled},
    {"nhl", "NHL", &AppSettings::rssSportNhlEnabled},
    {"ncaaf", "NCAAF", &AppSettings::rssSportNcaafEnabled},
    {"nfl", "NFL", &AppSettings::rssSportNflEnabled},
    {"nba", "NBA", &AppSettings::rssSportNbaEnabled},
    {"big10", "BIG10", &AppSettings::rssSportBig10Enabled},
};

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

String trimCopy(const char* input) {
  if (input == nullptr) {
    return String();
  }
  String value = input;
  value.trim();
  return value;
}

bool startsWithHttp(const String& value) {
  return value.startsWith("http://") || value.startsWith("https://");
}

bool containsPhpPath(const String& value) {
  String lower = value;
  lower.toLowerCase();
  return lower.indexOf(".php") >= 0;
}

void replaceAll(String& value, const char* from, const char* to) {
  value.replace(from, to);
}

bool buildSportsUrl(const AppSettings& settings, const char* sportKey,
                    String& outUrl) {
  String base = trimCopy(settings.rssSportsBaseUrl);
  if (base.length() == 0) {
    return false;
  }

  if (!startsWithHttp(base)) {
    base = String("https://") + base;
  }

  if (!containsPhpPath(base)) {
    if (!base.endsWith("/")) {
      base += "/";
    }
    base += "espn_scores_rss.php";
  } else {
    // Normalize to backend script name while still requesting JSON output.
    replaceAll(base, "espn_scores_json.php", "espn_scores_rss.php");
  }

  outUrl = base;
  if (outUrl.indexOf('?') >= 0) {
    outUrl += "&";
  } else {
    outUrl += "?";
  }
  outUrl += "sport=";
  outUrl += sportKey;
  outUrl += "&format=json";
  return true;
}
}  // namespace

size_t buildRssSources(const AppSettings& settings, RssSource* outSources,
                       size_t maxSources) {
  if (outSources == nullptr || maxSources == 0 || !settings.rssEnabled) {
    return 0;
  }

  size_t count = 0;

  auto addSportsSources = [&]() {
    if (!settings.rssSportsEnabled) {
      return;
    }
    for (const SportDef& sport : kSports) {
      if (!(settings.*(sport.enabledField))) {
        continue;
      }
      if (count >= maxSources) {
        break;
      }

      String sportUrl;
      if (!buildSportsUrl(settings, sport.key, sportUrl)) {
        continue;
      }

      safeCopy(outSources[count].name, sizeof(outSources[count].name), sport.label);
      safeCopy(outSources[count].url, sizeof(outSources[count].url), sportUrl.c_str());
      outSources[count].enabled = true;
      count++;
    }
  };

  auto addNprSource = [&]() {
    if (!settings.rssNprEnabled || settings.rssUrl[0] == '\0' || count >= maxSources) {
      return;
    }
    safeCopy(outSources[count].name, sizeof(outSources[count].name), "NPR");
    safeCopy(outSources[count].url, sizeof(outSources[count].url), settings.rssUrl);
    outSources[count].enabled = true;
    count++;
  };

  // Ordered mode expected sequence:
  // mlb, nhl, ncaaf, nfl, nba, big10, npr
  if (settings.rssRandomEnabled) {
    addNprSource();
    addSportsSources();
  } else {
    addSportsSources();
    addNprSource();
  }

  return count;
}

bool hasEnabledRssSources(const AppSettings& settings) {
  RssSource sources[APP_MAX_RSS_SOURCES];
  return buildRssSources(settings, sources, APP_MAX_RSS_SOURCES) > 0;
}
