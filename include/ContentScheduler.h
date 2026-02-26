#ifndef CONTENT_SCHEDULER_H
#define CONTENT_SCHEDULER_H

#include <Arduino.h>

#include "DisplayPanel.h"
#include "Scroller.h"

enum class ContentMode {
  Messages,
  ConfigPrompt,
  RssPlayback,
  Fallback,
};

struct ScheduledMessage {
  const char* text;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  bool enabled;
};

class ContentScheduler {
public:
  using RssSegmentProvider = bool (*)(String&, uint8_t&, uint8_t&, uint8_t&);

  ContentScheduler(Scroller& scroller, DisplayPanel& panel);

  void begin(const ScheduledMessage* messages, size_t messageCount,
             uint16_t messageDelayMs, uint8_t messagePixelsPerTick);
  void tick();
  void advanceNow();
  void updateMessages(const ScheduledMessage* messages, size_t messageCount);

  void setMode(ContentMode mode);
  ContentMode mode() const;

  void setMessageDelayMs(uint16_t messageDelayMs);
  uint16_t messageDelayMs() const;
  void setMessagePixelsPerTick(uint8_t messagePixelsPerTick);
  uint8_t messagePixelsPerTick() const;

  void setConfigPromptText(const String& text);
  void setRssPlaceholder(const String& title, const String& description);
  void setRssSegmentProvider(RssSegmentProvider provider);
  void setFallbackText(const String& text);

private:
  void startCurrentContent();
  bool startNextEnabledMessage();
  void startRssSegment();

  Scroller& _scroller;
  DisplayPanel& _panel;

  const ScheduledMessage* _messages;
  size_t _messageCount;
  size_t _nextMessageIndex;
  uint16_t _messageDelayMs;
  uint8_t _messagePixelsPerTick;
  ContentMode _mode;

  String _configPromptText;
  String _rssTitleText;
  String _rssDescriptionText;
  bool _rssShowTitleNext;
  RssSegmentProvider _rssSegmentProvider;
  String _fallbackText;
};

#endif
