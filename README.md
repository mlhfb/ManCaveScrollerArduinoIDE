# ManCaveScrollerArduinoIDE

Repository for a clean Arduino-framework rewrite of ManCaveScroller on ESP32.

Current status: Phase 9 implementation baseline is buildable (`pio run`, `pio run -t buildfs`).
After web UI changes, run `pio run -t uploadfs` so the device serves the updated `data/web/index.html`.

## Current Firmware Status
- Display pipeline: `FastLED` + `FastLED_NeoMatrix` on ESP32 Arduino.
- Font: Adafruit GFX built-in 5x7 (matching legacy `rssArduinoPlatform` look).
- Scroller behavior baseline: legacy `scrollMe()` style horizontal scroll bounds.
- Message playback: rotating multi-message playlist with per-message RGB colors.
- Scheduler: cooperative `ContentScheduler` with runtime modes (`messages`, `config`, `rss`, `fallback`) and cycle-complete handoff.
- Persistent settings: versioned JSON settings in LittleFS (`/config/settings.json`) with defaults and schema version.
- WiFi + config mode:
  - AP bootstrap when no saved STA credentials
  - STA connection flow with timeout/retry
  - BOOT button toggle for config mode
  - captive DNS redirect in AP mode
  - WiFi radio off during normal scrolling mode to reduce artifacts
  - Outside config mode, runtime suspends WiFi/web/RSS refresh tasks and prioritizes scroll output
  - In config mode, scrolling remains active while WiFi/web/API run simultaneously
  - On cold boot with configured WiFi+RSS, runtime starts immediate refresh while scrolling `Now Loading...`
  - After boot refresh completes, mode switch waits for current loading scroll cycle completion
  - Config prompt now scrolls active mode plus network details (`SSID` and `IP`)
- RSS fetch/parsing/sanitization:
  - HTTPS fetch with retry/backoff and timeout
  - bounded RSS XML parsing (`title`, `description`)
  - CDATA removal, HTML tag stripping, entity decode, UTF-8 to display-safe ASCII sanitize
- RSS cache/resilience:
  - per-source LittleFS cache files with metadata header
  - last-good cache retained on feed failures
  - non-repeating random picker across enabled sources until cycle exhaustion
  - periodic refresh schedule (15 min) with retry interval (60 sec) on failures (config mode runtime)
  - `LIVE` flag inference hook for sports hot-list prioritization
  - selectable playback mode:
    - random mode (default OFF): no-repeat random across enabled sources
    - ordered mode: iterate sources in configured UI order and items in source order
  - ordered mode refreshes each source at source-cycle start:
    - in config mode: uses active STA connection
    - outside config mode: radio cycles on, fetches next source/news, radio cycles off
  - items with empty description display title only (description segment skipped)
- Web API: endpoint contract implemented (`/api/status`, messages/text/color, speed/brightness/appearance, wifi, advanced, rss, factory-reset), including `rss_source_count` + `rss_sources[]` cache metadata in status.
- Web UI served from LittleFS (`/web/index.html`) for full setup:
  - message editing (5 slots)
  - appearance sliders
  - WiFi credentials
  - advanced panel settings
  - RSS + sports source configuration
  - playback-order randomization toggle (`Randomize RSS/sports item order (shuffle, OFF by default)`)
  - factory reset confirmation
  - non-destructive status refresh loop (does not overwrite unsaved form edits while configuring)
  - UI build stamp and no-cache root response headers to reduce stale page confusion
- Serial test controls:
  - Brightness: `u` (up), `d` (down)
  - Speed (delay only): `f` (faster), `s` (slower), `1..9` and `0` for exact speed 1..10
  - Pixel step: `p` toggles `1 -> 2 -> 3`
  - Scheduler mode testing: `m`/`r`/`b` manual override, `a` auto mode
  - Help: `h`
- Scroller defaults on boot:
  - Delay: `0 ms` (speed `10`)
  - Pixel step: `1`
- Scroller timing path uses `FastLED.delay()`
- LittleFS mount path explicitly uses partition label `littlefs` to match `partitions.csv`.
- RSS refresh path is stack-safe on Arduino `loopTask` (fetch buffer is persistent, not local-stack allocated).
- Sports fetch path now targets JSON backend responses (`espn_scores_json.php` + `format=json`) and parses JSON directly.

## Objective
Create a new version of the scroller that:
- Uses Arduino on ESP32 DoIt DevKit V1 (same practical hardware basis as `rssArduinoPlatform`)
- Preserves and improves smooth text scrolling on WS2812 matrix panels
- Includes functionality and features from `mlhfb/ManCaveScroller`
- Avoids legacy architecture issues (scope misuse, unmanaged globals, pointer hazards)

## Reference Inputs Studied
- Local reference: `C:\Users\mikelch\Documents\Projects\rssArduinoPlatform`
  - Platform/board/framework baseline
  - Matrix hardware details and scroll rendering style
  - Existing pin usage and practical runtime constraints
- Feature reference: `https://github.com/mlhfb/ManCaveScroller`
  - User-visible feature set
  - API contract and settings model
  - RSS/news/sports behavior, caching and fallback strategy
  - Config mode and WiFi flow expectations

## Planned Platform Baseline
- Board: `esp32doit-devkit-v1`
- Framework: `arduino`
- Display: WS2812B chained 8x32 modules (32/64/96/128 columns)
- Filesystem: LittleFS
- Config persistence: versioned settings store (LittleFS/NVS-backed strategy)

## Feature Scope (Target Parity)
- Smooth scrolling text renderer with legacy-style step bounds and runtime delay control
- Up to 5 custom messages with per-message color and enable flags
- Web UI for message, appearance, WiFi, advanced, RSS configuration
- AP + STA WiFi behavior with captive portal in AP mode
- BOOT button config mode toggle
- RSS feed support:
  - NPR-style news feed
  - Sports feeds (`mlb`, `nhl`, `ncaaf`, `nfl`, `nba`, `big10`) via backend URL strategy
- Cache-backed RSS playback with resilient fallback to custom messages
- Factory reset endpoint and persistent settings across reboot

## API Surface (Baseline Implemented)
- `GET /api/status`
- `POST /api/messages`
- `POST /api/text` (legacy)
- `POST /api/color` (legacy)
- `POST /api/speed`
- `POST /api/brightness`
- `POST /api/appearance`
- `POST /api/wifi`
- `POST /api/advanced`
- `POST /api/rss`
- `POST /api/factory-reset`

`GET /api/status` additionally reports:
- `rss_source_count`
- `rss_sources[]` with `name`, `url`, `enabled`, `cache_valid`, `cache_item_count`, `cache_updated_epoch`
- `rss_random_enabled`

## Architecture
- `main.cpp` orchestrator only (state machine driven)
- Display module (panel mapping, brightness, panel width)
- Scroller module (legacy-style stepping with runtime `FastLED.delay()` speed profile)
- Settings module (LittleFS defaults/load/save, schema handling)
- WiFi module (AP/STA, config-mode radio control, captive DNS)
- Web module (WebServer handlers + JSON validation)
- RSS fetcher module (HTTPS + XML extraction + sanitize)
- RSS cache module (per-source storage + no-repeat picker)
- Content scheduler module (message/RSS arbitration and refresh timing)

## Current Modules In Repo
- `include/AppConfig.h`
- `include/AppTypes.h`
- `include/DisplayPanel.h` + `src/DisplayPanel.cpp`
- `include/Scroller.h` + `src/Scroller.cpp`
- `include/ContentScheduler.h` + `src/ContentScheduler.cpp`
- `include/SettingsStore.h` + `src/SettingsStore.cpp`
- `include/WifiService.h` + `src/WifiService.cpp`
- `include/WebService.h` + `src/WebService.cpp`
- `include/RssSources.h` + `src/RssSources.cpp`
- `include/RssSanitizer.h` + `src/RssSanitizer.cpp`
- `include/RssFetcher.h` + `src/RssFetcher.cpp`
- `include/RssCache.h` + `src/RssCache.cpp`
- `include/RssRuntime.h` + `src/RssRuntime.cpp`
- `src/main.cpp`

## Milestones
1. Project scaffold and build config
2. Display + scroller core
3. Runtime speed/brightness controls + message rotation
4. Scheduler scaffolding for mode arbitration
5. Settings + WiFi/config mode baseline
6. Web API + UI baseline
7. RSS fetch + parse + cache
8. Integration, validation, and first tagged release candidate

Detailed checklist lives in `TODO.md`.
