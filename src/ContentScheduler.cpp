#include "ContentScheduler.h"

ContentScheduler::ContentScheduler(Scroller& scroller, DisplayPanel& panel)
    : _scroller(scroller),
      _panel(panel),
      _messages(nullptr),
      _messageCount(0),
      _nextMessageIndex(0),
      _messageDelayMs(8),
      _messagePixelsPerTick(1),
      _mode(ContentMode::Messages),
      _configPromptText("Config mode"),
      _rssTitleText("RSS placeholder title"),
      _rssDescriptionText("RSS placeholder description"),
      _rssShowTitleNext(true),
      _rssSegmentProvider(nullptr),
      _fallbackText("Fallback mode") {}

void ContentScheduler::begin(const ScheduledMessage* messages, size_t messageCount,
                             uint16_t messageDelayMs,
                             uint8_t messagePixelsPerTick) {
  _messages = messages;
  _messageCount = messageCount;
  _nextMessageIndex = 0;
  _messageDelayMs = messageDelayMs;
  _messagePixelsPerTick = messagePixelsPerTick;
  _rssShowTitleNext = true;
  _scroller.setStepDelayMs(_messageDelayMs);
  _scroller.setPixelsPerTick(_messagePixelsPerTick);
  startCurrentContent();
}

void ContentScheduler::tick() {
  if (_scroller.cycleComplete()) {
    _scroller.clearCycleComplete();
    startCurrentContent();
  }
}

void ContentScheduler::updateMessages(const ScheduledMessage* messages,
                                      size_t messageCount) {
  _messages = messages;
  _messageCount = messageCount;
  _nextMessageIndex = 0;
  if (_mode == ContentMode::Messages || _mode == ContentMode::Fallback) {
    startCurrentContent();
  }
}

void ContentScheduler::setMode(ContentMode mode) {
  _mode = mode;
  startCurrentContent();
}

ContentMode ContentScheduler::mode() const { return _mode; }

void ContentScheduler::setMessageDelayMs(uint16_t messageDelayMs) {
  _messageDelayMs = messageDelayMs;
  _scroller.setStepDelayMs(_messageDelayMs);
}

uint16_t ContentScheduler::messageDelayMs() const { return _messageDelayMs; }

void ContentScheduler::setMessagePixelsPerTick(uint8_t messagePixelsPerTick) {
  _messagePixelsPerTick = messagePixelsPerTick;
  _scroller.setPixelsPerTick(_messagePixelsPerTick);
}

uint8_t ContentScheduler::messagePixelsPerTick() const {
  return _messagePixelsPerTick;
}

void ContentScheduler::setConfigPromptText(const String& text) {
  _configPromptText = text;
}

void ContentScheduler::setRssPlaceholder(const String& title,
                                         const String& description) {
  _rssTitleText = title;
  _rssDescriptionText = description;
}

void ContentScheduler::setRssSegmentProvider(RssSegmentProvider provider) {
  _rssSegmentProvider = provider;
}

void ContentScheduler::setFallbackText(const String& text) { _fallbackText = text; }

void ContentScheduler::startCurrentContent() {
  if (_mode == ContentMode::Messages) {
    if (!startNextEnabledMessage()) {
      _scroller.start("No enabled messages",
                      _panel.color(255, 0, 0), _messageDelayMs);
    }
    return;
  }

  if (_mode == ContentMode::ConfigPrompt) {
    _scroller.start(_configPromptText, _panel.color(255, 195, 0), _messageDelayMs);
    return;
  }

  if (_mode == ContentMode::RssPlayback) {
    startRssSegment();
    return;
  }

  // Fallback mode: prefer user messages; if none, show fallback status text.
  if (!startNextEnabledMessage()) {
    _scroller.start(_fallbackText, _panel.color(255, 0, 0), _messageDelayMs);
  }
}

bool ContentScheduler::startNextEnabledMessage() {
  if (_messages == nullptr || _messageCount == 0) {
    return false;
  }

  for (size_t i = 0; i < _messageCount; i++) {
    const size_t idx = (_nextMessageIndex + i) % _messageCount;
    const ScheduledMessage& m = _messages[idx];

    if (!m.enabled || m.text == nullptr || m.text[0] == '\0') {
      continue;
    }

    _nextMessageIndex = (idx + 1) % _messageCount;
    _scroller.start(m.text, _panel.color(m.r, m.g, m.b), _messageDelayMs);
    return true;
  }

  return false;
}

void ContentScheduler::startRssSegment() {
  if (_rssSegmentProvider != nullptr) {
    String text;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    if (_rssSegmentProvider(text, r, g, b) && text.length() > 0) {
      _scroller.start(text, _panel.color(r, g, b), _messageDelayMs);
      return;
    }
  }

  if (_rssShowTitleNext) {
    _scroller.start(_rssTitleText, _panel.color(245, 245, 245), _messageDelayMs);
  } else {
    _scroller.start(_rssDescriptionText, _panel.color(0, 255, 0), _messageDelayMs);
  }
  _rssShowTitleNext = !_rssShowTitleNext;
}
