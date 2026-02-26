#include "Scroller.h"

#include <FastLED.h>

Scroller::Scroller(DisplayPanel& panel)
    : _panel(panel),
      _text(""),
      _charColors{},
      _textPixelWidth(0),
      _x(0),
      _color(0),
      _stepDelayMs(0),
      _pixelsPerTick(1),
      _usePerCharColors(false),
      _active(false),
      _cycleComplete(false) {}

void Scroller::start(const String& text, uint16_t color, uint16_t stepDelayMs) {
  parseInlineColorMarkup(text, color);
  _textPixelWidth = static_cast<uint16_t>(_text.length() * 6);
  _x = static_cast<int16_t>(_panel.width());
  _color = color;
  _stepDelayMs = stepDelayMs;
  _active = true;
  _cycleComplete = false;
}

void Scroller::stop() {
  _active = false;
  _panel.clear();
  _panel.show();
}

void Scroller::tick() {
  if (!_active) {
    return;
  }

  _panel.clear();
  if (_usePerCharColors) {
    _panel.drawTextAtColored(_x, _text.c_str(), _charColors, _text.length(),
                             _color);
  } else {
    _panel.drawTextAt(_x, _text.c_str(), _color);
  }
  _panel.show();

  if (_stepDelayMs > 0) {
    FastLED.delay(_stepDelayMs);
  }

  _x -= _pixelsPerTick;
  if (_x < -static_cast<int16_t>(_textPixelWidth)) {
    _active = false;
    _cycleComplete = true;
    _panel.clear();
    _panel.show();
  }
}

void Scroller::setStepDelayMs(uint16_t stepDelayMs) { _stepDelayMs = stepDelayMs; }

uint16_t Scroller::stepDelayMs() const { return _stepDelayMs; }

void Scroller::setPixelsPerTick(uint8_t pixelsPerTick) {
  _pixelsPerTick = pixelsPerTick;
  if (_pixelsPerTick == 0) {
    _pixelsPerTick = 1;
  }
}

uint8_t Scroller::pixelsPerTick() const { return _pixelsPerTick; }

bool Scroller::isActive() const { return _active; }

bool Scroller::cycleComplete() const { return _cycleComplete; }

void Scroller::clearCycleComplete() { _cycleComplete = false; }

bool Scroller::parseHexColorTag(const String& text, size_t offset,
                                uint16_t& outColor) const {
  // Tag format: [[#RRGGBB]]
  if (offset + 11 > text.length()) {
    return false;
  }
  if (!(text.startsWith("[[#", offset) && text.startsWith("]]", offset + 9))) {
    return false;
  }

  auto hexNibble = [](char c) -> int8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };

  const int8_t h0 = hexNibble(text[offset + 3]);
  const int8_t h1 = hexNibble(text[offset + 4]);
  const int8_t h2 = hexNibble(text[offset + 5]);
  const int8_t h3 = hexNibble(text[offset + 6]);
  const int8_t h4 = hexNibble(text[offset + 7]);
  const int8_t h5 = hexNibble(text[offset + 8]);
  if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0 || h5 < 0) {
    return false;
  }

  const uint8_t r = static_cast<uint8_t>((h0 << 4) | h1);
  const uint8_t g = static_cast<uint8_t>((h2 << 4) | h3);
  const uint8_t b = static_cast<uint8_t>((h4 << 4) | h5);
  outColor = _panel.color(r, g, b);
  return true;
}

void Scroller::parseInlineColorMarkup(const String& text, uint16_t defaultColor) {
  _text = "";
  _text.reserve(text.length());
  _usePerCharColors = false;

  uint16_t currentColor = defaultColor;
  size_t outIndex = 0;

  for (size_t i = 0; i < text.length(); i++) {
    uint16_t parsedColor = defaultColor;
    if (parseHexColorTag(text, i, parsedColor)) {
      currentColor = parsedColor;
      _usePerCharColors = true;
      i += 10;
      continue;
    }

    if (i + 5 <= text.length() && text.startsWith("[[/]]", i)) {
      currentColor = defaultColor;
      _usePerCharColors = true;
      i += 4;
      continue;
    }

    if (outIndex >= kMaxRenderedChars - 1) {
      break;
    }

    _text += text[i];
    _charColors[outIndex] = currentColor;
    outIndex++;
  }
}
