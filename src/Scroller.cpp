#include "Scroller.h"

#include <FastLED.h>

Scroller::Scroller(DisplayPanel& panel)
    : _panel(panel),
      _text(""),
      _textPixelWidth(0),
      _x(0),
      _color(0),
      _stepDelayMs(0),
      _pixelsPerTick(1),
      _active(false),
      _cycleComplete(false) {}

void Scroller::start(const String& text, uint16_t color, uint16_t stepDelayMs) {
  _text = text;
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
  _panel.drawTextAt(_x, _text.c_str(), _color);
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
