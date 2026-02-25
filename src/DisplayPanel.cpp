#include "DisplayPanel.h"

#include "AppConfig.h"

DisplayPanel::DisplayPanel(uint16_t width, uint8_t height)
    : _width(width),
      _height(height),
      _numLeds(width * height),
      _brightness(APP_DEFAULT_BRIGHTNESS),
      _leds(nullptr),
      _matrix(nullptr) {}

DisplayPanel::~DisplayPanel() {
  delete _matrix;
  delete[] _leds;
}

bool DisplayPanel::begin() {
  if (_width == 0 || _height == 0 || (_width % 8) != 0) {
    return false;
  }

  _leds = new CRGB[_numLeds];
  if (_leds == nullptr) {
    return false;
  }

  _matrix = new FastLED_NeoMatrix(
      _leds, 8, _height, _width / 8, 1,
      NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS +
          NEO_MATRIX_ZIGZAG);
  if (_matrix == nullptr) {
    return false;
  }

  FastLED.addLeds<NEOPIXEL, LED_STRIP_GPIO>(_leds, _numLeds);
  _matrix->begin();
  // Match rssArduinoPlatform: Adafruit GFX built-in 5x7 bitmap font.
  _matrix->setFont(nullptr);
  _matrix->setTextSize(1);
  _matrix->setTextWrap(false);
  _matrix->setBrightness(_brightness);
  _matrix->fillScreen(0);
  _matrix->show();
  return true;
}

void DisplayPanel::setBrightness(uint8_t brightness) {
  _brightness = brightness;
  if (_matrix != nullptr) {
    _matrix->setBrightness(_brightness);
  }
}

void DisplayPanel::clear() {
  if (_matrix != nullptr) {
    _matrix->fillScreen(0);
  }
}

void DisplayPanel::drawTextAt(int16_t x, const char* text, uint16_t colorValue) {
  if (_matrix == nullptr || text == nullptr) {
    return;
  }

  _matrix->setCursor(x, 0);
  _matrix->setTextColor(colorValue);
  _matrix->print(text);
}

void DisplayPanel::show() {
  if (_matrix != nullptr) {
    _matrix->show();
  }
}

uint16_t DisplayPanel::width() const { return _width; }

uint8_t DisplayPanel::height() const { return _height; }

uint16_t DisplayPanel::color(uint8_t r, uint8_t g, uint8_t b) const {
  if (_matrix == nullptr) {
    return 0;
  }
  return _matrix->Color(r, g, b);
}
