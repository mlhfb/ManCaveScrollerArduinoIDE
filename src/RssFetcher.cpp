#include "RssFetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <string.h>

#include "RssSanitizer.h"

namespace {
constexpr size_t kRssMaxResponseBytes = 64 * 1024;
constexpr uint8_t kJsonWalkMaxDepth = 5;

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

bool extractTagContent(const String& block, const char* tagName, String& out) {
  String open = String("<") + tagName + ">";
  String close = String("</") + tagName + ">";

  const int start = block.indexOf(open);
  if (start < 0) {
    return false;
  }
  const int contentStart = start + open.length();
  const int end = block.indexOf(close, contentStart);
  if (end < 0) {
    return false;
  }

  out = block.substring(contentStart, end);
  return true;
}

bool payloadLooksLikeJson(const String& payload) {
  for (size_t i = 0; i < payload.length(); i++) {
    const char c = payload[i];
    if (isspace(static_cast<unsigned char>(c))) {
      continue;
    }
    return c == '{' || c == '[';
  }
  return false;
}

bool urlRequestsJson(const char* url) {
  if (url == nullptr) {
    return false;
  }
  String lower = url;
  lower.toLowerCase();
  return lower.indexOf("format=json") >= 0 || lower.indexOf(".json") >= 0 ||
         lower.indexOf("_json.php") >= 0;
}

String jsonToString(JsonVariantConst value) {
  if (value.isNull()) {
    return String();
  }
  if (value.is<const char*>()) {
    return String(value.as<const char*>());
  }
  if (value.is<bool>()) {
    return value.as<bool>() ? String("true") : String("false");
  }
  if (value.is<long>()) {
    return String(value.as<long>());
  }
  if (value.is<unsigned long>()) {
    return String(value.as<unsigned long>());
  }
  if (value.is<float>()) {
    return String(value.as<float>(), 2);
  }
  if (value.is<double>()) {
    return String(value.as<double>(), 2);
  }
  return String();
}

bool readStringField(JsonObjectConst obj, const char* key, String& out) {
  if (obj.isNull() || key == nullptr) {
    return false;
  }
  JsonVariantConst v = obj[key];
  if (v.isNull()) {
    return false;
  }
  out = jsonToString(v);
  out.trim();
  return out.length() > 0;
}

bool extractTeamName(JsonVariantConst value, String& out) {
  if (value.isNull()) {
    return false;
  }
  if (value.is<const char*>()) {
    out = value.as<const char*>();
    out.trim();
    return out.length() > 0;
  }
  if (!value.is<JsonObjectConst>()) {
    return false;
  }

  JsonObjectConst obj = value.as<JsonObjectConst>();
  if (readStringField(obj, "displayName", out) ||
      readStringField(obj, "shortDisplayName", out) ||
      readStringField(obj, "abbreviation", out) ||
      readStringField(obj, "name", out)) {
    return true;
  }

  JsonVariantConst team = obj["team"];
  if (!team.isNull()) {
    return extractTeamName(team, out);
  }
  return false;
}

bool normalizeHexColorForTag(const String& rawColor, String& outHex) {
  String value = rawColor;
  value.trim();
  if (value.length() == 0) {
    return false;
  }
  if (value[0] == '#') {
    value.remove(0, 1);
  }
  if (value.length() != 6) {
    return false;
  }
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    const bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
    if (!isHex) {
      return false;
    }
  }
  value.toUpperCase();
  outHex = value;
  return true;
}

bool extractTeamColor(JsonVariantConst value, String& outHex) {
  outHex = "";
  if (value.isNull()) {
    return false;
  }
  if (value.is<const char*>()) {
    return normalizeHexColorForTag(value.as<const char*>(), outHex);
  }
  if (!value.is<JsonObjectConst>()) {
    return false;
  }

  JsonObjectConst obj = value.as<JsonObjectConst>();
  String candidate;
  if (readStringField(obj, "teamColor", candidate) ||
      readStringField(obj, "color", candidate) ||
      readStringField(obj, "primaryColor", candidate)) {
    if (normalizeHexColorForTag(candidate, outHex)) {
      return true;
    }
  }

  JsonVariantConst team = obj["team"];
  if (!team.isNull()) {
    return extractTeamColor(team, outHex);
  }
  return false;
}

bool extractScoreText(JsonVariantConst value, String& out) {
  if (value.isNull()) {
    return false;
  }
  if (value.is<JsonObjectConst>()) {
    JsonObjectConst obj = value.as<JsonObjectConst>();
    if (readStringField(obj, "score", out) || readStringField(obj, "value", out) ||
        readStringField(obj, "points", out)) {
      return true;
    }
    return false;
  }
  out = jsonToString(value);
  out.trim();
  return out.length() > 0;
}

bool parseScoreValue(const String& scoreText, int& outScore) {
  if (scoreText.length() == 0) {
    return false;
  }

  bool sawDigit = false;
  int value = 0;
  for (size_t i = 0; i < scoreText.length(); i++) {
    const char c = scoreText[i];
    if (c >= '0' && c <= '9') {
      sawDigit = true;
      value = value * 10 + (c - '0');
    } else if (sawDigit) {
      break;
    }
  }
  if (!sawDigit) {
    return false;
  }
  outScore = value;
  return true;
}

bool looksLikeLiveGameDetail(const String& detailText) {
  String lower = detailText;
  lower.toLowerCase();
  return lower.indexOf("top of") >= 0 || lower.indexOf("bottom of") >= 0 ||
         lower.indexOf("halftime") >= 0 || lower.indexOf("qtr") >= 0 ||
         lower.indexOf("quarter") >= 0 || lower.indexOf("period") >= 0 ||
         lower.indexOf("in progress") >= 0 || lower.indexOf("live") >= 0;
}

bool looksLikeFinalGameDetail(const String& detailText) {
  String lower = detailText;
  lower.toLowerCase();
  return lower.indexOf("final") >= 0 || lower.indexOf("completed") >= 0;
}

bool looksLikeScheduledDetail(const String& detailText) {
  String lower = detailText;
  lower.toLowerCase();

  if (looksLikeLiveGameDetail(detailText) || looksLikeFinalGameDetail(detailText)) {
    return false;
  }

  if (lower.indexOf("scheduled") >= 0 || lower.indexOf("pre-game") >= 0 ||
      lower.indexOf("pregame") >= 0) {
    return true;
  }

  const bool hasAt = lower.indexOf(" at ") >= 0;
  const bool hasClock = (lower.indexOf(":") >= 0) &&
                        (lower.indexOf(" am") >= 0 || lower.indexOf(" pm") >= 0);
  const bool hasTz = lower.indexOf(" est") >= 0 || lower.indexOf(" edt") >= 0 ||
                     lower.indexOf(" et") >= 0;

  return hasAt && (hasClock || hasTz);
}

String colorTagWrap(const String& text, const char* hexColor) {
  String out = "[[#";
  out += hexColor;
  out += "]]";
  out += text;
  out += "[[/]]";
  return out;
}

void formatSportsHeadline(const String& awayName, const String& homeName,
                          const String& awayTeamColorHex,
                          const String& homeTeamColorHex,
                          const String& awayScore, const String& homeScore,
                          bool showScores, String& outTitle) {
  String awayNameDisplay = awayName;
  if (awayTeamColorHex.length() > 0) {
    awayNameDisplay = colorTagWrap(awayName, awayTeamColorHex.c_str());
  }
  String homeNameDisplay = homeName;
  if (homeTeamColorHex.length() > 0) {
    homeNameDisplay = colorTagWrap(homeName, homeTeamColorHex.c_str());
  }

  if (!showScores || awayScore.length() == 0 || homeScore.length() == 0) {
    outTitle = awayNameDisplay + " at " + homeNameDisplay;
    return;
  }

  int awayValue = 0;
  int homeValue = 0;
  const bool haveAwayValue = parseScoreValue(awayScore, awayValue);
  const bool haveHomeValue = parseScoreValue(homeScore, homeValue);

  String awayScoreDisplay = awayScore;
  String homeScoreDisplay = homeScore;
  if (haveAwayValue && haveHomeValue && awayValue != homeValue) {
    awayScoreDisplay =
        colorTagWrap(awayScore, (awayValue > homeValue) ? "00FF00" : "FF0000");
    homeScoreDisplay =
        colorTagWrap(homeScore, (homeValue > awayValue) ? "00FF00" : "FF0000");
  }

  outTitle = awayNameDisplay + " " + awayScoreDisplay + " at " +
             homeNameDisplay + " " + homeScoreDisplay;
}

void appendPart(String& base, const String& part, const char* separator) {
  if (part.length() == 0) {
    return;
  }
  if (base.length() > 0 && separator != nullptr && separator[0] != '\0') {
    base += separator;
  }
  base += part;
}

bool readNestedStatus(JsonObjectConst obj, String& out) {
  if (obj.isNull()) {
    return false;
  }

  if (readStringField(obj, "status", out) || readStringField(obj, "state", out) ||
      readStringField(obj, "short_status", out) ||
      readStringField(obj, "game_status", out)) {
    return true;
  }

  JsonVariantConst status = obj["status"];
  if (!status.is<JsonObjectConst>()) {
    return false;
  }
  JsonObjectConst statusObj = status.as<JsonObjectConst>();
  if (readStringField(statusObj, "description", out) ||
      readStringField(statusObj, "detail", out)) {
    return true;
  }
  JsonVariantConst type = statusObj["type"];
  if (type.is<JsonObjectConst>()) {
    JsonObjectConst typeObj = type.as<JsonObjectConst>();
    if (readStringField(typeObj, "shortDetail", out) ||
        readStringField(typeObj, "detail", out) ||
        readStringField(typeObj, "description", out) ||
        readStringField(typeObj, "state", out)) {
      return true;
    }
  }
  return false;
}

bool extractEspnEvent(JsonObjectConst obj, String& outTitle, String& outDescription) {
  JsonArrayConst competitions = obj["competitions"].as<JsonArrayConst>();
  if (competitions.isNull() || competitions.size() == 0) {
    return false;
  }

  JsonObjectConst competition = competitions[0].as<JsonObjectConst>();
  JsonArrayConst competitors = competition["competitors"].as<JsonArrayConst>();
  if (competitors.isNull() || competitors.size() < 2) {
    return false;
  }

  String awayName;
  String homeName;
  String awayScore;
  String homeScore;
  String awayTeamColorHex;
  String homeTeamColorHex;
  String firstName;
  String secondName;
  String firstScore;
  String secondScore;
  String firstColorHex;
  String secondColorHex;
  bool haveFirst = false;
  bool haveSecond = false;

  for (JsonVariantConst competitorVar : competitors) {
    if (!competitorVar.is<JsonObjectConst>()) {
      continue;
    }
    JsonObjectConst competitor = competitorVar.as<JsonObjectConst>();

    String name;
    extractTeamName(competitor["team"], name);
    if (name.length() == 0) {
      extractTeamName(competitor, name);
    }

    String score;
    extractScoreText(competitor["score"], score);
    String teamColorHex;
    extractTeamColor(competitor["team"], teamColorHex);
    if (teamColorHex.length() == 0) {
      extractTeamColor(competitor, teamColorHex);
    }

    String homeAway;
    readStringField(competitor, "homeAway", homeAway);
    homeAway.toLowerCase();

    if (homeAway == "away") {
      awayName = name;
      awayScore = score;
      awayTeamColorHex = teamColorHex;
    } else if (homeAway == "home") {
      homeName = name;
      homeScore = score;
      homeTeamColorHex = teamColorHex;
    }

    if (!haveFirst && name.length() > 0) {
      firstName = name;
      firstScore = score;
      firstColorHex = teamColorHex;
      haveFirst = true;
    } else if (!haveSecond && name.length() > 0) {
      secondName = name;
      secondScore = score;
      secondColorHex = teamColorHex;
      haveSecond = true;
    }
  }

  if (awayName.length() == 0 || homeName.length() == 0) {
    if (!haveFirst || !haveSecond) {
      return false;
    }
    awayName = firstName;
    awayScore = firstScore;
    awayTeamColorHex = firstColorHex;
    homeName = secondName;
    homeScore = secondScore;
    homeTeamColorHex = secondColorHex;
  }

  String status;
  if (!readNestedStatus(competition, status)) {
    readNestedStatus(obj, status);
  }

  bool isLive = looksLikeLiveGameDetail(status);
  if (!isLive) {
    JsonVariantConst statusNode = competition["status"];
    if (statusNode.is<JsonObjectConst>()) {
      JsonObjectConst statusObj = statusNode.as<JsonObjectConst>();
      JsonVariantConst typeNode = statusObj["type"];
      if (typeNode.is<JsonObjectConst>()) {
        String state;
        if (readStringField(typeNode.as<JsonObjectConst>(), "state", state)) {
          state.toLowerCase();
          isLive = (state == "in" || state == "live");
        }
      }
    }
  }

  const bool showScores = awayScore.length() > 0 && homeScore.length() > 0 &&
                          !(looksLikeScheduledDetail(status) && !isLive);
  formatSportsHeadline(awayName, homeName, awayTeamColorHex, homeTeamColorHex,
                       awayScore, homeScore, showScores, outTitle);
  outDescription = status;

  return outTitle.length() > 0;
}

bool extractHomeAwayPair(JsonObjectConst obj, String& outTitle, String& outDescription) {
  String awayName;
  String homeName;
  String awayScore;
  String homeScore;
  String awayTeamColorHex;
  String homeTeamColorHex;

  const char* awayNameKeys[] = {"away_team", "away", "visitor", "team1"};
  const char* homeNameKeys[] = {"home_team", "home", "host", "team2"};
  const char* awayScoreKeys[] = {"away_score",  "awayScore",    "visitor_score",
                                 "score_away",  "team1_score",  "away_points"};
  const char* homeScoreKeys[] = {"home_score",  "homeScore",    "host_score",
                                 "score_home",  "team2_score",  "home_points"};
  const char* awayColorKeys[] = {"away_team_color", "awayColor", "visitor_color",
                                 "team1Color",      "away_color"};
  const char* homeColorKeys[] = {"home_team_color", "homeColor", "host_color",
                                 "team2Color",      "home_color"};

  for (const char* key : awayNameKeys) {
    if (extractTeamName(obj[key], awayName)) {
      break;
    }
  }
  for (const char* key : homeNameKeys) {
    if (extractTeamName(obj[key], homeName)) {
      break;
    }
  }
  for (const char* key : awayColorKeys) {
    String candidate;
    if (readStringField(obj, key, candidate) &&
        normalizeHexColorForTag(candidate, awayTeamColorHex)) {
      break;
    }
  }
  for (const char* key : homeColorKeys) {
    String candidate;
    if (readStringField(obj, key, candidate) &&
        normalizeHexColorForTag(candidate, homeTeamColorHex)) {
      break;
    }
  }

  if (awayName.length() == 0 || homeName.length() == 0) {
    JsonArrayConst teams = obj["teams"].as<JsonArrayConst>();
    if (!teams.isNull() && teams.size() >= 2) {
      String t1;
      String t2;
      extractTeamName(teams[0], t1);
      extractTeamName(teams[1], t2);
      if (awayName.length() == 0) awayName = t1;
      if (homeName.length() == 0) homeName = t2;
      if (awayTeamColorHex.length() == 0) {
        extractTeamColor(teams[0], awayTeamColorHex);
      }
      if (homeTeamColorHex.length() == 0) {
        extractTeamColor(teams[1], homeTeamColorHex);
      }
    }
  }

  if (awayName.length() == 0 || homeName.length() == 0) {
    return false;
  }

  for (const char* key : awayScoreKeys) {
    if (extractScoreText(obj[key], awayScore)) {
      break;
    }
  }
  for (const char* key : homeScoreKeys) {
    if (extractScoreText(obj[key], homeScore)) {
      break;
    }
  }

  // Support backend shape:
  // {"away":{"name":"...","score":1},"home":{"name":"...","score":2},"detail":"..."}
  if (awayScore.length() == 0 && obj["away"].is<JsonObjectConst>()) {
    JsonObjectConst awayObj = obj["away"].as<JsonObjectConst>();
    extractScoreText(awayObj["score"], awayScore);
    if (awayTeamColorHex.length() == 0) {
      extractTeamColor(awayObj, awayTeamColorHex);
    }
  }
  if (homeScore.length() == 0 && obj["home"].is<JsonObjectConst>()) {
    JsonObjectConst homeObj = obj["home"].as<JsonObjectConst>();
    extractScoreText(homeObj["score"], homeScore);
    if (homeTeamColorHex.length() == 0) {
      extractTeamColor(homeObj, homeTeamColorHex);
    }
  }

  String detail;
  if (!readStringField(obj, "detail", detail)) {
    readStringField(obj, "summary", detail);
  }
  if (detail.length() == 0) {
    readNestedStatus(obj, detail);
  }

  const bool isLive = (obj["isLive"].is<bool>() && obj["isLive"].as<bool>()) ||
                      (obj["live"].is<bool>() && obj["live"].as<bool>()) ||
                      looksLikeLiveGameDetail(detail);

  const bool showScores = awayScore.length() > 0 && homeScore.length() > 0 &&
                          !(looksLikeScheduledDetail(detail) && !isLive);
  formatSportsHeadline(awayName, homeName, awayTeamColorHex, homeTeamColorHex,
                       awayScore, homeScore, showScores, outTitle);
  outDescription = detail;
  return true;
}

bool parseJsonItemObject(JsonObjectConst obj, String& outTitle, String& outDescription) {
  if (obj.isNull()) {
    return false;
  }

  if (extractEspnEvent(obj, outTitle, outDescription)) {
    return true;
  }
  if (extractHomeAwayPair(obj, outTitle, outDescription)) {
    return outTitle.length() > 0;
  }

  const char* titleKeys[] = {"title", "headline", "matchup", "event", "game"};
  for (const char* key : titleKeys) {
    if (readStringField(obj, key, outTitle)) {
      break;
    }
  }
  if (outTitle.length() == 0) {
    return false;
  }

  const char* descriptionKeys[] = {"description", "summary", "details", "detail", "text"};
  for (const char* key : descriptionKeys) {
    if (readStringField(obj, key, outDescription)) {
      break;
    }
  }

  String status;
  if (readNestedStatus(obj, status)) {
    appendPart(outDescription, status, " | ");
  }

  String score;
  if (readStringField(obj, "score", score)) {
    appendPart(outDescription, score, " | ");
  }

  return true;
}

void collectJsonItems(JsonVariantConst node, RssItem* outItems, size_t maxItems,
                      uint16_t& count, uint8_t depth) {
  if (count >= maxItems || node.isNull() || depth > kJsonWalkMaxDepth) {
    return;
  }

  if (node.is<JsonArrayConst>()) {
    JsonArrayConst arr = node.as<JsonArrayConst>();
    for (JsonVariantConst child : arr) {
      collectJsonItems(child, outItems, maxItems, count, depth + 1);
      if (count >= maxItems) {
        return;
      }
    }
    return;
  }

  if (!node.is<JsonObjectConst>()) {
    return;
  }

  JsonObjectConst obj = node.as<JsonObjectConst>();
  String title;
  String description;
  if (parseJsonItemObject(obj, title, description)) {
    const String safeTitle = sanitizeRssText(title);
    const String safeDescription = sanitizeRssText(description);
    if (safeTitle.length() > 0) {
      safeCopy(outItems[count].title, sizeof(outItems[count].title), safeTitle.c_str());
      safeCopy(outItems[count].description, sizeof(outItems[count].description),
               safeDescription.c_str());
      String liveTest = safeTitle + " " + safeDescription;
      liveTest.toLowerCase();
      outItems[count].flags =
          (liveTest.indexOf("live") >= 0) ? RssItemFlagLive : RssItemFlagNone;
      count++;
      if (count >= maxItems) {
        return;
      }
    }
  }

  for (JsonPairConst kv : obj) {
    JsonVariantConst child = kv.value();
    if (child.is<JsonObjectConst>() || child.is<JsonArrayConst>()) {
      collectJsonItems(child, outItems, maxItems, count, depth + 1);
      if (count >= maxItems) {
        return;
      }
    }
  }
}
}  // namespace

RssFetcher::RssFetcher() {}

RssFetchResult RssFetcher::fetch(const char* url, RssItem* outItems,
                                 size_t maxItems, uint8_t maxAttempts,
                                 uint32_t timeoutMs, uint32_t backoffMs) const {
  RssFetchResult result = {false, 0, -1, ""};

  if (url == nullptr || url[0] == '\0') {
    result.error = "RSS URL is empty";
    return result;
  }
  if (outItems == nullptr || maxItems == 0) {
    result.error = "Output item buffer is invalid";
    return result;
  }

  for (size_t i = 0; i < maxItems; i++) {
    outItems[i].title[0] = '\0';
    outItems[i].description[0] = '\0';
    outItems[i].flags = RssItemFlagNone;
  }

  if (maxAttempts == 0) {
    maxAttempts = 1;
  }

  for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(static_cast<uint16_t>(timeoutMs));

    if (!http.begin(client, url)) {
      result.error = "HTTP begin failed";
    } else {
      const int status = http.GET();
      result.httpStatus = status;

      if (status == HTTP_CODE_OK) {
        String payload = http.getString();
        if (payload.length() > kRssMaxResponseBytes) {
          payload.remove(kRssMaxResponseBytes);
        }

        const bool preferJson = urlRequestsJson(url) || payloadLooksLikeJson(payload);
        uint16_t count = 0;
        if (preferJson) {
          count = parseJsonFeed(payload, outItems, maxItems);
          if (count == 0) {
            count = parseRssXml(payload, outItems, maxItems);
          }
        } else {
          count = parseRssXml(payload, outItems, maxItems);
          if (count == 0 && payloadLooksLikeJson(payload)) {
            count = parseJsonFeed(payload, outItems, maxItems);
          }
        }

        if (count > 0) {
          result.success = true;
          result.itemCount = count;
          result.error = "";
          http.end();
          return result;
        }

        result.error = "No feed items parsed";
      } else {
        result.error = String("HTTP status ") + status;
      }
      http.end();
    }

    if (attempt < maxAttempts) {
      delay(backoffMs * attempt);
    }
  }

  return result;
}

uint16_t RssFetcher::parseRssXml(const String& xml, RssItem* outItems,
                                 size_t maxItems) const {
  uint16_t count = 0;
  int searchPos = 0;

  while (count < maxItems) {
    const int itemStart = xml.indexOf("<item", searchPos);
    if (itemStart < 0) {
      break;
    }

    const int itemOpenEnd = xml.indexOf('>', itemStart);
    if (itemOpenEnd < 0) {
      break;
    }

    const int itemEnd = xml.indexOf("</item>", itemOpenEnd + 1);
    if (itemEnd < 0) {
      break;
    }

    const String itemBlock = xml.substring(itemOpenEnd + 1, itemEnd);
    String rawTitle;
    String rawDescription;

    extractTagContent(itemBlock, "title", rawTitle);
    extractTagContent(itemBlock, "description", rawDescription);

    const String title = sanitizeRssText(rawTitle);
    const String description = sanitizeRssText(rawDescription);

    if (title.length() > 0) {
      safeCopy(outItems[count].title, sizeof(outItems[count].title), title.c_str());
      safeCopy(outItems[count].description, sizeof(outItems[count].description),
               description.c_str());
      outItems[count].flags = RssItemFlagNone;
      count++;
    }

    searchPos = itemEnd + 7;
  }

  return count;
}

uint16_t RssFetcher::parseJsonFeed(const String& payload, RssItem* outItems,
                                   size_t maxItems) const {
  if (outItems == nullptr || maxItems == 0) {
    return 0;
  }

  size_t capacity = payload.length() + 4096;
  if (capacity < 8192) {
    capacity = 8192;
  } else if (capacity > 96 * 1024) {
    capacity = 96 * 1024;
  }

  DynamicJsonDocument doc(capacity);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    return 0;
  }

  uint16_t count = 0;
  collectJsonItems(doc.as<JsonVariantConst>(), outItems, maxItems, count, 0);
  return count;
}
