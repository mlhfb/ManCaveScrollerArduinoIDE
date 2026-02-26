# Include Directory

Current module headers:
- `AppConfig.h` - hardware and runtime speed profile constants
- `AppTypes.h` - shared settings/messages/RSS types
- `DisplayPanel.h` - matrix panel abstraction
- `Scroller.h` - legacy-style scrolling engine
- `ContentScheduler.h` - non-blocking content arbitration modes
- `SettingsStore.h` - LittleFS settings persistence and defaults
- `WifiService.h` - AP/STA and captive DNS runtime control
- `WebService.h` - HTTP API routing and JSON handlers
- `RssSources.h` - RSS source manifest build from settings
- `RssSanitizer.h` - RSS text cleanup helpers
- `RssFetcher.h` - HTTPS feed fetch + parse interface
- `RssCache.h` - LittleFS per-source cache and no-repeat picker
- `RssRuntime.h` - refresh scheduling and RSS playback runtime

Runtime defaults are defined in `AppConfig.h`:
- speed default maps to `0 ms` delay
- pixel step default is `1`

LittleFS note:
- Settings mount call uses explicit partition label `littlefs`.
