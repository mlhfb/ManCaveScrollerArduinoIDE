#include "RssFetcher.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

#include "RssSanitizer.h"

namespace {
constexpr size_t kRssMaxResponseBytes = 64 * 1024;

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

        const uint16_t count = parseRssXml(payload, outItems, maxItems);
        if (count > 0) {
          result.success = true;
          result.itemCount = count;
          result.error = "";
          http.end();
          return result;
        }

        result.error = "No RSS items parsed";
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
