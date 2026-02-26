#include <Arduino.h>

#include "AppConfig.h"
#include "AppTypes.h"
#include "ContentScheduler.h"
#include "DisplayPanel.h"
#include "RssRuntime.h"
#include "Scroller.h"
#include "SettingsStore.h"
#include "WebService.h"
#include "WifiService.h"

namespace {
constexpr uint8_t kConfigButtonPin = 0;  // BOOT button on ESP32 DevKit
constexpr uint8_t kEncoderButtonPin = 35;  // External encoder button (active-low)
constexpr uint32_t kButtonDebounceMs = 250;
constexpr uint32_t kConfigPromptRefreshMs = 2000;
constexpr uint32_t kBootRefreshTaskStackWords = 6144;
constexpr BaseType_t kBootRefreshTaskPriority = 1;
constexpr const char* kBootLoadingText = "Now Loading...";
}

SettingsStore gSettingsStore;
WifiService gWifiService;
RssRuntime gRssRuntime(gSettingsStore, gWifiService);
DisplayPanel gDisplay(APP_MATRIX_WIDTH, APP_MATRIX_HEIGHT);
Scroller gScroller(gDisplay);
ContentScheduler gScheduler(gScroller, gDisplay);
WebService gWebService(gSettingsStore, gWifiService);

ScheduledMessage gScheduledMessages[APP_MAX_MESSAGES];

uint8_t gBrightness = APP_DEFAULT_BRIGHTNESS;
uint8_t gScrollSpeed = APP_SCROLL_SPEED_DEFAULT;
uint8_t gPixelStep = APP_SCROLL_PIXEL_STEP_DEFAULT;
bool gConfigMode = false;
bool gManualModeOverride = false;
ContentMode gManualMode = ContentMode::Messages;
String gLastConfigPrompt;
uint32_t gLastConfigPromptMs = 0;

bool gLastButtonState = true;
uint32_t gLastButtonChangeMs = 0;
bool gLastEncoderButtonState = true;
uint32_t gLastEncoderButtonChangeMs = 0;

volatile bool gExitConfigRequested = false;

bool gBootLoadingMode = false;
volatile bool gBootRefreshComplete = false;
volatile bool gBootRefreshSuccess = false;
TaskHandle_t gBootRefreshTaskHandle = nullptr;

void printStatus();

void bootRefreshTask(void* /*param*/) {
  const bool refreshed = gRssRuntime.refreshAllNow();
  gBootRefreshSuccess = refreshed;
  gBootRefreshComplete = true;
  gBootRefreshTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void syncSchedulerMessagesFromSettings() {
  const AppSettings& settings = gSettingsStore.settings();
  for (size_t i = 0; i < APP_MAX_MESSAGES; i++) {
    gScheduledMessages[i].text = settings.messages[i].text;
    gScheduledMessages[i].r = settings.messages[i].r;
    gScheduledMessages[i].g = settings.messages[i].g;
    gScheduledMessages[i].b = settings.messages[i].b;
    gScheduledMessages[i].enabled = settings.messages[i].enabled;
  }
  gScheduler.updateMessages(gScheduledMessages, APP_MAX_MESSAGES);
}

bool shouldUseRssPlayback() {
  const AppSettings& settings = gSettingsStore.settings();
  if (!settings.rssEnabled || !gRssRuntime.hasEnabledSources()) {
    return false;
  }

  // Random mode requires existing cache; ordered mode can refresh per-source on demand.
  if (settings.rssRandomEnabled) {
    return gRssRuntime.cacheReady();
  }

  if (gRssRuntime.cacheReady()) {
    return true;
  }
  return settings.wifiSsid[0] != '\0';
}

void applySchedulerMode() {
  if (gBootLoadingMode) {
    if (gScheduler.mode() != ContentMode::ConfigPrompt) {
      gScheduler.setMode(ContentMode::ConfigPrompt);
    }
    return;
  }

  if (gConfigMode) {
    if (gScheduler.mode() != ContentMode::ConfigPrompt) {
      gScheduler.setMode(ContentMode::ConfigPrompt);
    }
    return;
  }

  if (gManualModeOverride) {
    if (gScheduler.mode() != gManualMode) {
      gScheduler.setMode(gManualMode);
    }
    return;
  }

  const ContentMode target =
      shouldUseRssPlayback() ? ContentMode::RssPlayback : ContentMode::Fallback;
  if (gScheduler.mode() != target) {
    gScheduler.setMode(target);
  }
}

void refreshConfigPromptText(bool forceRestart) {
  String mode = gWifiService.modeString();
  String ssid = gWifiService.ssid();
  String ip = gWifiService.ip();

  String text = "Config ";
  text += mode;
  text += " SSID:";
  text += (ssid.length() > 0) ? ssid : "(none)";
  text += " IP:";
  text += ip;

  if (forceRestart || text != gLastConfigPrompt) {
    gLastConfigPrompt = text;
    gScheduler.setConfigPromptText(text);
    if (gScheduler.mode() == ContentMode::ConfigPrompt) {
      gScheduler.setMode(ContentMode::ConfigPrompt);
    }
  }
  gLastConfigPromptMs = millis();
}

void applyRuntimeFromSettings(const AppSettings& settings) {
  gBrightness = settings.brightness;
  gScrollSpeed = settings.speed;
  if (gScrollSpeed < APP_SCROLL_SPEED_MIN) gScrollSpeed = APP_SCROLL_SPEED_MIN;
  if (gScrollSpeed > APP_SCROLL_SPEED_MAX) gScrollSpeed = APP_SCROLL_SPEED_MAX;

  gDisplay.setBrightness(gBrightness);
  gScheduler.setMessageDelayMs(appScrollDelayForSpeed(gScrollSpeed));
  gScheduler.setMessagePixelsPerTick(gPixelStep);
  syncSchedulerMessagesFromSettings();
  gRssRuntime.onSettingsChanged(settings);
  applySchedulerMode();
}

bool shouldRunBootRefresh() {
  const AppSettings& settings = gSettingsStore.settings();
  if (settings.wifiSsid[0] == '\0') {
    return false;
  }
  if (!settings.rssEnabled) {
    return false;
  }
  return gRssRuntime.hasEnabledSources();
}

void beginBootLoadingRefresh() {
  if (!shouldRunBootRefresh()) {
    gBootLoadingMode = false;
    return;
  }

  gBootLoadingMode = true;
  gBootRefreshComplete = false;
  gBootRefreshSuccess = false;
  gScheduler.setConfigPromptText(kBootLoadingText);
  applySchedulerMode();

  if (xTaskCreatePinnedToCore(bootRefreshTask, "boot_refresh",
                              kBootRefreshTaskStackWords, nullptr,
                              kBootRefreshTaskPriority, &gBootRefreshTaskHandle,
                              tskNO_AFFINITY) != pdPASS) {
    gBootLoadingMode = false;
    gBootRefreshComplete = true;
    gBootRefreshSuccess = false;
    gBootRefreshTaskHandle = nullptr;
    gScheduler.setConfigPromptText("Config mode active");
    applySchedulerMode();
    Serial.println("Boot refresh task start failed");
    return;
  }

  Serial.println("Boot refresh started");
}

void completeBootLoadingIfReady(bool cycleComplete) {
  if (!gBootLoadingMode || !gBootRefreshComplete || !cycleComplete) {
    return;
  }

  gBootLoadingMode = false;
  gRssRuntime.queueStartupWeather();
  gScheduler.setConfigPromptText("Config mode active");
  applySchedulerMode();
  Serial.print("Boot refresh done: ");
  Serial.println(gBootRefreshSuccess ? "fresh content updated" : "using cache/fallback");
  printStatus();
}

bool provideRssSegment(String& text, uint8_t& r, uint8_t& g, uint8_t& b) {
  return gRssRuntime.nextSegment(text, r, g, b);
}

void printStatus() {
  Serial.print("Brightness=");
  Serial.print(gBrightness);
  Serial.print(" Speed=");
  Serial.print(gScrollSpeed);
  Serial.print(" DelayMs=");
  Serial.print(appScrollDelayForSpeed(gScrollSpeed));
  Serial.print(" StepPx=");
  Serial.print(gPixelStep);
  Serial.print(" WifiMode=");
  Serial.print(gWifiService.modeString());
  Serial.print(" IP=");
  Serial.print(gWifiService.ip());
  Serial.print(" ConfigMode=");
  Serial.print(gConfigMode ? "on" : "off");
  Serial.print(" RSSSources=");
  Serial.print(gRssRuntime.sourceCount());
  Serial.print(" RSSCache=");
  Serial.print(gRssRuntime.hasCachedContent() ? "ready" : "empty");
  Serial.print(" Mode=");
  switch (gScheduler.mode()) {
    case ContentMode::Messages:
      Serial.print("messages");
      break;
    case ContentMode::ConfigPrompt:
      Serial.print("config");
      break;
    case ContentMode::RssPlayback:
      Serial.print("rss");
      break;
    case ContentMode::Fallback:
    default:
      Serial.print("fallback");
      break;
  }
  Serial.println();
}

void printSerialHelp() {
  Serial.println("Controls:");
  Serial.println("  u=brightness up, d=brightness down");
  Serial.println("  f=speed faster, s=speed slower");
  Serial.println("  1..9=set speed 1..9, 0=set speed 10");
  Serial.println("  p=toggle pixel step (1/2/3)");
  Serial.println("  n=advance to next scroll item");
  Serial.println("  c=enter config mode, x=exit config mode");
  Serial.println("  m=manual messages, r=manual rss, b=manual fallback, a=auto");
  Serial.println("  h=help");
  printStatus();
}

void saveAndApplySettings() {
  gSettingsStore.mutableSettings().brightness = gBrightness;
  gSettingsStore.mutableSettings().speed = gScrollSpeed;
  gSettingsStore.save();
  applyRuntimeFromSettings(gSettingsStore.settings());
}

void applyScrollSpeed() {
  gScheduler.setMessageDelayMs(appScrollDelayForSpeed(gScrollSpeed));
  gSettingsStore.mutableSettings().speed = gScrollSpeed;
  gSettingsStore.save();
  Serial.print("Speed now ");
  Serial.print(gScrollSpeed);
  Serial.print(" (delay ");
  Serial.print(appScrollDelayForSpeed(gScrollSpeed));
  Serial.println(" ms)");
}

void applyPixelStep() {
  gScheduler.setMessagePixelsPerTick(gPixelStep);
  Serial.print("Pixel step now ");
  Serial.println(gPixelStep);
}

void enterConfigMode() {
  if (gConfigMode) return;
  if (gBootLoadingMode && !gBootRefreshComplete) {
    Serial.println("Boot refresh still running; wait for Now Loading... to finish");
    return;
  }
  if (gBootLoadingMode) {
    gBootLoadingMode = false;
    gScheduler.setConfigPromptText("Config mode active");
  }
  gConfigMode = true;
  gWifiService.enterConfigMode(gSettingsStore.settings());
  if (!gWebService.isRunning()) {
    gWebService.begin();
  }
  gRssRuntime.setSuspended(false);
  gRssRuntime.setRadioControlEnabled(false);
  gRssRuntime.forceRefreshSoon();
  refreshConfigPromptText(true);
  applySchedulerMode();
  Serial.println("Entered config mode");
  printStatus();
}

void exitConfigMode() {
  if (!gConfigMode) return;
  gConfigMode = false;
  gWebService.stop();
  gWifiService.exitConfigMode(true);
  gRssRuntime.setRadioControlEnabled(true);
  gRssRuntime.setSuspended(true);
  applySchedulerMode();
  Serial.println("Exited config mode");
  printStatus();
}

void onSettingsChanged(const AppSettings& settings) {
  applyRuntimeFromSettings(settings);
}

void onWifiConnectRequested() { gWifiService.enterConfigMode(gSettingsStore.settings()); }

void onFactoryResetRequested() {
  gSettingsStore.factoryReset();
  ESP.restart();
}

void onExitConfigRequested() { gExitConfigRequested = true; }

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'u') {
      gBrightness = static_cast<uint8_t>(min(255, gBrightness + 8));
      gDisplay.setBrightness(gBrightness);
      saveAndApplySettings();
      Serial.print("Brightness: ");
      Serial.println(gBrightness);
    } else if (c == 'd') {
      gBrightness = static_cast<uint8_t>(max(0, gBrightness - 8));
      gDisplay.setBrightness(gBrightness);
      saveAndApplySettings();
      Serial.print("Brightness: ");
      Serial.println(gBrightness);
    } else if (c == 'f') {
      if (gScrollSpeed < APP_SCROLL_SPEED_MAX) gScrollSpeed++;
      applyScrollSpeed();
    } else if (c == 's') {
      if (gScrollSpeed > APP_SCROLL_SPEED_MIN) gScrollSpeed--;
      applyScrollSpeed();
    } else if (c >= '1' && c <= '9') {
      gScrollSpeed = static_cast<uint8_t>(c - '0');
      applyScrollSpeed();
    } else if (c == '0') {
      gScrollSpeed = 10;
      applyScrollSpeed();
    } else if (c == 'p') {
      gPixelStep++;
      if (gPixelStep > APP_SCROLL_PIXEL_STEP_MAX) {
        gPixelStep = APP_SCROLL_PIXEL_STEP_MIN;
      }
      applyPixelStep();
    } else if (c == 'n') {
      Serial.println("[SCROLL] Manual advance requested");
      gScheduler.advanceNow();
    } else if (c == 'c') {
      enterConfigMode();
    } else if (c == 'x') {
      exitConfigMode();
    } else if (c == 'm') {
      gManualModeOverride = true;
      gManualMode = ContentMode::Messages;
      applySchedulerMode();
      printStatus();
    } else if (c == 'r') {
      gManualModeOverride = true;
      gManualMode = ContentMode::RssPlayback;
      applySchedulerMode();
      printStatus();
    } else if (c == 'b') {
      gManualModeOverride = true;
      gManualMode = ContentMode::Fallback;
      applySchedulerMode();
      printStatus();
    } else if (c == 'a') {
      gManualModeOverride = false;
      applySchedulerMode();
      printStatus();
    } else if (c == 'h') {
      printSerialHelp();
    }
  }
}

void handleConfigButton() {
  const uint32_t now = millis();
  auto processButton = [&](uint8_t pin, bool& lastState, uint32_t& lastChangeMs) {
    const bool buttonState = digitalRead(pin);
    if (buttonState == lastState) {
      return;
    }
    if ((now - lastChangeMs) <= kButtonDebounceMs) {
      return;
    }

    lastChangeMs = now;
    lastState = buttonState;

    // Legacy encoder library in rssArduinoPlatform treats button as active-low.
    if (!buttonState) {
      if (gConfigMode) {
        exitConfigMode();
      } else {
        enterConfigMode();
      }
    }
  };

  processButton(kConfigButtonPin, gLastButtonState, gLastButtonChangeMs);
  processButton(kEncoderButtonPin, gLastEncoderButtonState,
                gLastEncoderButtonChangeMs);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ManCaveScrollerArduinoIDE");
  Serial.println("RSS/cache runtime integration build");

  pinMode(kConfigButtonPin, INPUT_PULLUP);
  pinMode(kEncoderButtonPin, INPUT);
  gLastButtonState = digitalRead(kConfigButtonPin);
  gLastEncoderButtonState = digitalRead(kEncoderButtonPin);

  if (!gSettingsStore.begin()) {
    Serial.println("Settings/LittleFS init failed");
    while (true) {
      delay(1000);
    }
  }

  if (!gDisplay.begin()) {
    Serial.println("Display init failed");
    while (true) {
      delay(1000);
    }
  }

  gWebService.setOnSettingsChanged(onSettingsChanged);
  gWebService.setOnWifiConnectRequested(onWifiConnectRequested);
  gWebService.setOnFactoryResetRequested(onFactoryResetRequested);
  gWebService.setOnExitConfigRequested(onExitConfigRequested);
  gWebService.setRssRuntime(&gRssRuntime);

  gWifiService.begin();
  if (!gRssRuntime.begin()) {
    Serial.println("RSS runtime init failed");
  }

  const bool hasSavedWifi = gSettingsStore.settings().wifiSsid[0] != '\0';
  const bool bootRefreshPlanned = hasSavedWifi && shouldRunBootRefresh();
  gBootLoadingMode = bootRefreshPlanned;

  if (bootRefreshPlanned) {
    gScheduler.setConfigPromptText(kBootLoadingText);
    gScheduler.setMode(ContentMode::ConfigPrompt);
  } else {
    gScheduler.setConfigPromptText("Config mode active");
  }

  applyRuntimeFromSettings(gSettingsStore.settings());
  gScheduler.setRssPlaceholder("Loading RSS feed cache", "Using message fallback");
  gScheduler.setRssSegmentProvider(provideRssSegment);
  gScheduler.setFallbackText("RSS unavailable fallback");
  gScheduler.begin(gScheduledMessages, APP_MAX_MESSAGES,
                   appScrollDelayForSpeed(gScrollSpeed), gPixelStep);

  // Performance mode: run WiFi/web only in config mode.
  // Boot directly into AP/config mode only when no WiFi credentials exist.
  if (!hasSavedWifi) {
    gWifiService.startAp();
    gConfigMode = true;
    gWebService.begin();
    gRssRuntime.setSuspended(false);
    gRssRuntime.setRadioControlEnabled(false);
    gRssRuntime.forceRefreshSoon();
    refreshConfigPromptText(true);
    applySchedulerMode();
  } else {
    // Normal scrolling mode: WiFi off and RSS refresh suspended for max smoothness.
    gWifiService.stopWifi();
    gRssRuntime.setRadioControlEnabled(true);
    gRssRuntime.setSuspended(true);
    if (bootRefreshPlanned) {
      beginBootLoadingRefresh();
    } else {
      applySchedulerMode();
    }
  }

  printSerialHelp();
}

void loop() {
  handleSerialInput();
  handleConfigButton();
  if (gExitConfigRequested) {
    gExitConfigRequested = false;
    exitConfigMode();
  }

  if (gConfigMode) {
    gRssRuntime.setSuspended(false);
    gRssRuntime.tick();
    gWifiService.tick();
    gWebService.tick();
    if ((millis() - gLastConfigPromptMs) > kConfigPromptRefreshMs) {
      refreshConfigPromptText(false);
    }
  } else {
    gRssRuntime.setSuspended(true);
  }

  gScroller.tick();
  const bool cycleComplete = gScroller.cycleComplete();
  completeBootLoadingIfReady(cycleComplete);
  gScheduler.tick();
  if (!gConfigMode && !gManualModeOverride) {
    applySchedulerMode();
  }

  if (gConfigMode) {
    delay(1);
  } else {
    delay(0);
  }
}
