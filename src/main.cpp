#include <Arduino.h>

#include "AppConfig.h"
#include "ContentScheduler.h"
#include "DisplayPanel.h"
#include "Scroller.h"

DisplayPanel gDisplay(APP_MATRIX_WIDTH, APP_MATRIX_HEIGHT);
Scroller gScroller(gDisplay);
ContentScheduler gScheduler(gScroller, gDisplay);

ScheduledMessage gMessages[] = {
    {"Phase 3 test message: scrolling is active", 0, 255, 255, true},
    {"Welcome to ManCaveScroller Arduino rewrite", 255, 195, 0, true},
    {"Serial: u/d brightness, f/s speed, h help", 0, 255, 0, true},
};

constexpr size_t kMessageCount = sizeof(gMessages) / sizeof(gMessages[0]);

uint8_t gBrightness = APP_DEFAULT_BRIGHTNESS;
uint8_t gScrollSpeed = APP_SCROLL_SPEED_DEFAULT;

void printStatus() {
  Serial.print("Brightness=");
  Serial.print(gBrightness);
  Serial.print(" Speed=");
  Serial.print(gScrollSpeed);
  Serial.print(" DelayMs=");
  Serial.print(appScrollDelayForSpeed(gScrollSpeed));
  Serial.print(" StepPx=");
  Serial.print(appScrollPixelsForSpeed(gScrollSpeed));
  Serial.print(" Mode=");
  switch (gScheduler.mode()) {
    case ContentMode::Messages:
      Serial.println("messages");
      break;
    case ContentMode::ConfigPrompt:
      Serial.println("config");
      break;
    case ContentMode::RssPlayback:
      Serial.println("rss-placeholder");
      break;
    case ContentMode::Fallback:
      Serial.println("fallback");
      break;
  }
}

void printSerialHelp() {
  Serial.println("Controls:");
  Serial.println("  u=brightness up, d=brightness down");
  Serial.println("  f=speed faster, s=speed slower");
  Serial.println("  1..9=set speed 1..9, 0=set speed 10");
  Serial.println("  speed 10 uses 0 ms delay");
  Serial.println("  m=messages, c=config text, r=rss placeholder, x=fallback");
  Serial.println("  h=help");
  printStatus();
}

void applyScrollSpeed() {
  gScheduler.setMessageDelayMs(appScrollDelayForSpeed(gScrollSpeed));
  gScheduler.setMessagePixelsPerTick(appScrollPixelsForSpeed(gScrollSpeed));
  Serial.print("Speed now ");
  Serial.print(gScrollSpeed);
  Serial.print(" (delay ");
  Serial.print(appScrollDelayForSpeed(gScrollSpeed));
  Serial.print(" ms, step ");
  Serial.print(appScrollPixelsForSpeed(gScrollSpeed));
  Serial.println(" px)");
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'u') {
      gBrightness = static_cast<uint8_t>(min(255, gBrightness + 8));
      gDisplay.setBrightness(gBrightness);
      Serial.print("Brightness: ");
      Serial.println(gBrightness);
    } else if (c == 'd') {
      gBrightness = static_cast<uint8_t>(max(0, gBrightness - 8));
      gDisplay.setBrightness(gBrightness);
      Serial.print("Brightness: ");
      Serial.println(gBrightness);
    } else if (c == 'f') {
      if (gScrollSpeed < APP_SCROLL_SPEED_MAX) {
        gScrollSpeed++;
      }
      applyScrollSpeed();
    } else if (c == 's') {
      if (gScrollSpeed > APP_SCROLL_SPEED_MIN) {
        gScrollSpeed--;
      }
      applyScrollSpeed();
    } else if (c >= '1' && c <= '9') {
      gScrollSpeed = static_cast<uint8_t>(c - '0');
      applyScrollSpeed();
    } else if (c == '0') {
      gScrollSpeed = 10;
      applyScrollSpeed();
    } else if (c == 'm') {
      gScheduler.setMode(ContentMode::Messages);
      printStatus();
    } else if (c == 'c') {
      gScheduler.setMode(ContentMode::ConfigPrompt);
      printStatus();
    } else if (c == 'r') {
      gScheduler.setMode(ContentMode::RssPlayback);
      printStatus();
    } else if (c == 'x') {
      gScheduler.setMode(ContentMode::Fallback);
      printStatus();
    } else if (c == 'h') {
      printSerialHelp();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ManCaveScrollerArduinoIDE");
  Serial.println("Phase 4 scheduler + runtime speed/brightness controls");

  if (!gDisplay.begin()) {
    Serial.println("Display init failed");
    while (true) {
      delay(1000);
    }
  }

  gDisplay.setBrightness(gBrightness);
  gScheduler.setConfigPromptText("Config mode placeholder");
  gScheduler.setRssPlaceholder("RSS placeholder title", "RSS placeholder description");
  gScheduler.setFallbackText("RSS unavailable fallback");
  gScheduler.begin(gMessages, kMessageCount,
                   appScrollDelayForSpeed(gScrollSpeed),
                   appScrollPixelsForSpeed(gScrollSpeed));
  printSerialHelp();
}

void loop() {
  handleSerialInput();
  gScroller.tick();
  gScheduler.tick();

  delay(1);
}
