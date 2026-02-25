#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>

#ifndef LED_STRIP_GPIO
#define LED_STRIP_GPIO 5
#endif

constexpr uint16_t APP_MATRIX_WIDTH = 128;
constexpr uint8_t APP_MATRIX_HEIGHT = 8;
constexpr uint8_t APP_DEFAULT_BRIGHTNESS = 72;

// Legacy scrollMe() baseline behavior with tunable step cadence (smaller = faster).
constexpr uint8_t APP_SCROLL_SPEED_MIN = 1;
constexpr uint8_t APP_SCROLL_SPEED_MAX = 10;
constexpr uint8_t APP_SCROLL_SPEED_DEFAULT = 6;

constexpr uint16_t APP_SCROLL_DELAY_BY_SPEED_MS[10] = {
    120,  // speed 1
    95,   // speed 2
    75,   // speed 3
    55,   // speed 4
    38,   // speed 5
    24,   // speed 6
    14,   // speed 7
    8,    // speed 8
    3,    // speed 9
    0,    // speed 10 (fastest)
};

constexpr uint8_t APP_SCROLL_PIXELS_BY_SPEED[10] = {
    1,  // speed 1
    1,  // speed 2
    1,  // speed 3
    1,  // speed 4
    2,  // speed 5
    2,  // speed 6
    3,  // speed 7
    4,  // speed 8
    5,  // speed 9
    6,  // speed 10
};

inline uint16_t appScrollDelayForSpeed(uint8_t speed) {
  if (speed < APP_SCROLL_SPEED_MIN) {
    speed = APP_SCROLL_SPEED_MIN;
  } else if (speed > APP_SCROLL_SPEED_MAX) {
    speed = APP_SCROLL_SPEED_MAX;
  }
  return APP_SCROLL_DELAY_BY_SPEED_MS[speed - 1];
}

inline uint8_t appScrollPixelsForSpeed(uint8_t speed) {
  if (speed < APP_SCROLL_SPEED_MIN) {
    speed = APP_SCROLL_SPEED_MIN;
  } else if (speed > APP_SCROLL_SPEED_MAX) {
    speed = APP_SCROLL_SPEED_MAX;
  }
  return APP_SCROLL_PIXELS_BY_SPEED[speed - 1];
}

#endif
