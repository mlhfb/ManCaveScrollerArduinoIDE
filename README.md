# ManCaveScrollerArduinoIDE

Repository for a clean Arduino-framework rewrite of ManCaveScroller on ESP32.

Current status: Phase 4 scaffolding is implemented and buildable.

## Current Firmware Status
- Display pipeline: `FastLED` + `FastLED_NeoMatrix` on ESP32 Arduino.
- Font: Adafruit GFX built-in 5x7 (matching legacy `rssArduinoPlatform` look).
- Scroller behavior baseline: legacy `scrollMe()` style horizontal scroll bounds.
- Message playback: rotating multi-message playlist with per-message RGB colors.
- Scheduler: cooperative `ContentScheduler` with runtime modes:
  - `messages`
  - `config` placeholder
  - `rss` placeholder (title/description alternation)
  - `fallback`
- Serial test controls:
  - Brightness: `u` (up), `d` (down)
  - Speed: `f` (faster), `s` (slower), `1..9` and `0` for exact speed 1..10
  - Help: `h`
- Speed profile now uses large visual deltas:
  - Delay + pixel-step per tick mapping
  - Fastest speed (`10`) uses `0 ms` delay
  - Scroller timing path uses `FastLED.delay()`

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
- Smooth scrolling text renderer with fractional speed stepping
- Up to 5 custom messages with per-message color and enable flags
- Web UI for message, appearance, WiFi, advanced, RSS configuration
- AP + STA WiFi behavior with captive portal in AP mode
- BOOT button config mode toggle
- RSS feed support:
  - NPR-style news feed
  - Sports feeds (`mlb`, `nhl`, `ncaaf`, `nfl`, `nba`, `big10`) via backend URL strategy
- Cache-backed RSS playback with resilient fallback to custom messages
- Factory reset endpoint and persistent settings across reboot

## Planned API Surface
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

## Planned Architecture
- `main.cpp` orchestrator only (state machine driven)
- Display module (panel mapping, brightness, panel width)
- Scroller module (legacy-style stepping with runtime delay/pixel-step profile)
- Settings module (defaults, load/save, schema handling)
- WiFi module (AP/STA, radio cycling, captive DNS)
- Web module (HTTP handlers + JSON validation)
- RSS fetcher module (HTTPS + XML extraction + sanitize)
- RSS cache module (per-source storage + no-repeat picker)
- Content scheduler module (message/RSS arbitration and refresh timing)

## Current Modules In Repo
- `include/AppConfig.h`
- `include/DisplayPanel.h` + `src/DisplayPanel.cpp`
- `include/Scroller.h` + `src/Scroller.cpp`
- `include/ContentScheduler.h` + `src/ContentScheduler.cpp`
- `src/main.cpp`

## Milestones
1. Project scaffold and build config
2. Display + scroller core
3. Runtime speed/brightness controls + message rotation
4. Scheduler scaffolding for mode arbitration
5. Settings + WiFi/config mode
6. Web API + UI
7. RSS fetch + parse + cache
8. Integration, validation, and first tagged release candidate

Detailed checklist lives in `TODO.md`.
