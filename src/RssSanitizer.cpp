#include "RssSanitizer.h"

#include <ctype.h>
#include <stdlib.h>

namespace {
struct EntityMap {
  const char* entity;
  const char* replacement;
};

constexpr EntityMap kEntityTable[] = {
    {"&amp;", "&"},
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&quot;", "\""},
    {"&apos;", "'"},
    {"&nbsp;", " "},
    {"&mdash;", "-"},
    {"&ndash;", "-"},
    {"&rsquo;", "'"},
    {"&lsquo;", "'"},
    {"&rdquo;", "\""},
    {"&ldquo;", "\""},
    {"&hellip;", "..."},
    {"&copy;", "(c)"},
    {"&reg;", "(R)"},
    {"&trade;", "(TM)"},
    {"&deg;", "deg"},
};

String stripCdata(const String& input) {
  String out = input;
  out.replace("<![CDATA[", "");
  out.replace("]]>", "");
  return out;
}

String stripHtmlTags(const String& input) {
  String out;
  out.reserve(input.length());

  bool inTag = false;
  for (size_t i = 0; i < input.length(); i++) {
    const char ch = input[i];
    if (ch == '<') {
      inTag = true;
      continue;
    }
    if (ch == '>' && inTag) {
      inTag = false;
      continue;
    }
    if (!inTag) {
      out += ch;
    }
  }
  return out;
}

bool decodeNumericEntity(const String& entity, String& replacement) {
  if (!entity.startsWith("&#") || !entity.endsWith(";")) {
    return false;
  }

  String body = entity.substring(2, entity.length() - 1);
  int base = 10;
  if (body.startsWith("x") || body.startsWith("X")) {
    base = 16;
    body.remove(0, 1);
  }
  if (body.length() == 0) {
    return false;
  }

  long codepoint = strtol(body.c_str(), nullptr, base);
  if (codepoint >= 32 && codepoint <= 126) {
    replacement = static_cast<char>(codepoint);
    return true;
  }

  // Common UTF-8 punctuation codepoints mapped to display-safe ASCII.
  switch (codepoint) {
    case 0x2013:
    case 0x2014:
      replacement = "-";
      return true;
    case 0x2018:
    case 0x2019:
      replacement = "'";
      return true;
    case 0x201C:
    case 0x201D:
      replacement = "\"";
      return true;
    case 0x2022:
      replacement = "*";
      return true;
    case 0x2026:
      replacement = "...";
      return true;
    default:
      replacement = "?";
      return true;
  }
}

String decodeEntities(const String& input) {
  String out;
  out.reserve(input.length());

  size_t i = 0;
  while (i < input.length()) {
    if (input[i] != '&') {
      out += input[i++];
      continue;
    }

    const int semi = input.indexOf(';', i);
    if (semi < 0 || (semi - static_cast<int>(i)) > 12) {
      out += input[i++];
      continue;
    }

    const String candidate = input.substring(i, semi + 1);
    String decoded;
    bool matched = false;

    for (const EntityMap& entry : kEntityTable) {
      if (candidate.equals(entry.entity)) {
        decoded = entry.replacement;
        matched = true;
        break;
      }
    }

    if (!matched) {
      matched = decodeNumericEntity(candidate, decoded);
    }

    if (matched) {
      out += decoded;
      i = static_cast<size_t>(semi + 1);
    } else {
      out += input[i++];
    }
  }

  return out;
}

String utf8ToDisplayAscii(const String& input) {
  String out;
  out.reserve(input.length());

  bool lastWasSpace = false;
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(input.c_str());
  size_t i = 0;
  const size_t len = input.length();

  while (i < len) {
    const uint8_t b0 = bytes[i];

    if (b0 >= 32 && b0 <= 126) {
      if (b0 == ' ') {
        if (!lastWasSpace) {
          out += ' ';
          lastWasSpace = true;
        }
      } else {
        out += static_cast<char>(b0);
        lastWasSpace = false;
      }
      i++;
      continue;
    }

    if (b0 < 0xC0) {
      i++;
      continue;
    }

    int seqLen = 0;
    if ((b0 & 0xE0) == 0xC0) {
      seqLen = 2;
    } else if ((b0 & 0xF0) == 0xE0) {
      seqLen = 3;
    } else if ((b0 & 0xF8) == 0xF0) {
      seqLen = 4;
    } else {
      i++;
      continue;
    }

    if (i + static_cast<size_t>(seqLen - 1) >= len) {
      break;
    }

    if (seqLen == 3 && b0 == 0xE2 && bytes[i + 1] == 0x80) {
      const uint8_t b2 = bytes[i + 2];
      if (b2 == 0x93 || b2 == 0x94) {
        out += '-';
      } else if (b2 == 0x98 || b2 == 0x99) {
        out += '\'';
      } else if (b2 == 0x9C || b2 == 0x9D) {
        out += '"';
      } else if (b2 == 0xA2) {
        out += '*';
      } else if (b2 == 0xA6) {
        out += "...";
      }
      lastWasSpace = false;
    }

    i += static_cast<size_t>(seqLen);
  }

  out.trim();
  return out;
}
}  // namespace

String sanitizeRssText(const String& input) {
  String out = stripCdata(input);
  out = stripHtmlTags(out);
  out = decodeEntities(out);
  out = utf8ToDisplayAscii(out);
  out.trim();
  return out;
}
