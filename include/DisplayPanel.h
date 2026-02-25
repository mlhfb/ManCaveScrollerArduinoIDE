#ifndef DISPLAY_PANEL_H
#define DISPLAY_PANEL_H

#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

class DisplayPanel {
public:
  DisplayPanel(uint16_t width, uint8_t height);
  ~DisplayPanel();

  bool begin();
  void setBrightness(uint8_t brightness);
  void clear();
  void drawTextAt(int16_t x, const char* text, uint16_t color);
  void show();

  uint16_t width() const;
  uint8_t height() const;
  uint16_t color(uint8_t r, uint8_t g, uint8_t b) const;

private:
  uint16_t _width;
  uint8_t _height;
  uint16_t _numLeds;
  uint8_t _brightness;

  CRGB* _leds;
  FastLED_NeoMatrix* _matrix;
};

#endif
