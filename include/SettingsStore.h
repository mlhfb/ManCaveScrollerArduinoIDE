#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <Arduino.h>

#include "AppTypes.h"

class SettingsStore {
public:
  bool begin();

  AppSettings& mutableSettings();
  const AppSettings& settings() const;

  bool load();
  bool save() const;
  void loadDefaults();
  bool factoryReset();

private:
  bool loadDefaultMessagesFromFile();
  void sanitize();

  AppSettings _settings;
};

#endif
