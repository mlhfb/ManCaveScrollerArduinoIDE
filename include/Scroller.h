#ifndef SCROLLER_H
#define SCROLLER_H

#include <Arduino.h>

#include "DisplayPanel.h"

class Scroller {
public:
  explicit Scroller(DisplayPanel& panel);

  void start(const String& text, uint16_t color, uint16_t stepDelayMs);
  void stop();
  void tick();
  void setStepDelayMs(uint16_t stepDelayMs);
  uint16_t stepDelayMs() const;
  void setPixelsPerTick(uint8_t pixelsPerTick);
  uint8_t pixelsPerTick() const;

  bool isActive() const;
  bool cycleComplete() const;
  void clearCycleComplete();

private:
  DisplayPanel& _panel;

  String _text;
  uint16_t _textPixelWidth;
  int16_t _x;
  uint16_t _color;
  uint16_t _stepDelayMs;
  uint8_t _pixelsPerTick;
  bool _active;
  bool _cycleComplete;
};

#endif
