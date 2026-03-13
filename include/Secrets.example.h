#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// Copy this file to include/Secrets.h and set your private values.
// include/Secrets.h is ignored by git.

#define APP_WEATHER_API_URL \
  "https://api.openweathermap.org/data/2.5/weather?id=4997384&appid=YOUR_API_KEY_HERE&mode=xml&units=imperial"

// HTTPS endpoint returning OTA manifest JSON.
// Example: https://your-domain.example/ota/manifest.json
#define APP_OTA_MANIFEST_URL "https://charlie.servebeer.com/ota/manifest.json"

#endif  // SECRETS_EXAMPLE_H
