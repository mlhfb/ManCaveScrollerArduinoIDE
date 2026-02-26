# CLAUDE.md

## Project Intent
Build a clean, modular Arduino-based rewrite of ManCaveScroller in this repository.

Use references as follows:
- `C:\Users\mikelch\Documents\Projects\rssArduinoPlatform`
  - Reference only for platform/board/hardware wiring and practical text scrolling on WS2812 matrix.
- `https://github.com/mlhfb/ManCaveScroller`
  - Reference for required user-facing features, behavior, and API surface.

Do not port spaghetti patterns, broad globals, or pointer-unsafe code from legacy sources.

## Required Baseline
- Platform: ESP32 DoIt DevKit V1
- Framework: Arduino
- Display type: WS2812B matrix, 8 rows, chained panel widths 32/64/96/128
- Matrix layout: column-major serpentine/zigzag

## Must-Have Feature Parity
- Smooth scrolling text with per-message color and speed/brightness controls
- Up to 5 messages with enable/disable and automatic cycling
- AP + STA WiFi behavior with captive portal in AP mode
- BOOT-button config mode toggle
- Web UI + JSON API endpoints matching current ManCaveScroller contract
- Persistent settings across reboot
- RSS news + sports source support with resilient fallback behavior
- LittleFS-hosted assets and cache-backed RSS playback

## Architecture Principles
- Non-blocking runtime:
  - `loop()` should orchestrate state machines, not block for full-message scroll cycles.
- Separation of concerns:
  - Display driver, scroller, settings, WiFi, web API, RSS fetcher, RSS cache, scheduler.
- Memory safety first:
  - Bounded buffers, explicit truncation, no unchecked `String` growth in hot paths.
  - Avoid large temporary stack allocations inside `loop()` path; prefer static/member buffers for RSS payload/item staging.
- Deterministic behavior:
  - Stable source order, explicit retry intervals, reproducible fallback rules.

## Proposed Module Layout
- `include/AppTypes.h` (shared models/enums)
- `include/DisplayPanel.h` / `src/DisplayPanel.cpp`
- `include/Scroller.h` / `src/Scroller.cpp`
- `include/SettingsStore.h` / `src/SettingsStore.cpp`
- `include/WifiService.h` / `src/WifiService.cpp`
- `include/WebService.h` / `src/WebService.cpp`
- `include/RssSources.h` / `src/RssSources.cpp` (materialized source manifest from settings)
- `include/RssSanitizer.h` / `src/RssSanitizer.cpp` (CDATA/tag/entity/UTF-8 cleanup)
- `include/RssFetcher.h` / `src/RssFetcher.cpp`
- `include/RssCache.h` / `src/RssCache.cpp`
- `include/RssRuntime.h` / `src/RssRuntime.cpp` (refresh scheduling + cache-backed playback)
- `include/ContentScheduler.h` / `src/ContentScheduler.cpp`
- `src/main.cpp` as orchestration only

## API Compatibility Target
Implement and maintain:
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

Web server robustness:
- Handle common captive/probe paths gracefully to avoid noisy not-found logs.
- `/api/rss` should accept both compact keys (`enabled`, `sports_enabled`, etc.) and explicit `rss_*` keys for compatibility.
- `/api/rss` includes `random_enabled` / `rss_random_enabled` playback mode toggle.
- Web UI should clearly expose the randomization toggle in Advanced playback settings.
- Randomization default should be OFF unless explicitly enabled by saved settings.

## Display/Smoothness Requirements
- Use Arduino `loop()` with FastLED-driven render cadence (`FastLED.show()`).
- Use simple scroll position stepping tuned for smooth visual motion.
- Runtime speed control uses `FastLED.delay()`; fastest speed (`10`) uses `0 ms` delay by default.
- Pixel-step changes are controlled independently (test toggle: `1/2/3`).
- Behavioral baseline comes from `rssArduinoPlatform` legacy `scrollMe()` path (`src/main.cpp` in committed history), not the uncommitted refactor copy.
- Cycle-complete callback/flag so scheduler switches content without tearing.
- Keep WiFi radio off during normal STA scrolling when possible to reduce artifacts.
- Scroll-priority runtime policy:
  - outside config mode, prioritize scrolling and suspend WiFi/web/RSS refresh tasks
  - inside config mode, keep scrolling active while WiFi/web are enabled for setup changes
- Cold-boot loading policy:
  - when RSS + WiFi are configured, run an immediate fresh-content refresh at startup
  - during refresh, scroll `Now Loading...`
  - after refresh completes, switch to normal scheduling only at a scroll cycle boundary
- Config prompt text should always include reachable network context (mode + SSID + IP).

## Data/Persistence Requirements
- Versioned settings schema with sane defaults.
- Loss-tolerant read path:
  - Corrupt or missing config falls back safely, does not crash boot.
- Cache data separated from settings data.
- LittleFS mount label must match partition table name (`littlefs` in this repo).

## Definition of Done (Initial Rewrite)
- Builds and flashes via PlatformIO on Arduino framework.
- Web UI can configure messages, appearance, WiFi, advanced settings, RSS.
- Device reboots with settings intact.
- RSS outages fall back to cached data or custom messages automatically.
- Smooth scrolling remains stable during long runtime.

## Current Implementation Notes (2026-02-26)
- RSS runtime now supports:
  - periodic refresh interval (`15 min`) and retry (`60 sec`)
  - per-source LittleFS cache files with metadata
  - random no-repeat playback pool across enabled cached sources
  - `LIVE` flag inference hook for future source prioritization
- `/api/status` includes `rss_source_count` and `rss_sources[]` cache metadata.
- RSS playback mode is selectable:
  - random no-repeat mode
  - ordered source/item mode with per-source refresh before cycle
    - config mode: refresh over active connection
    - non-config mode: refresh by cycling radio on/off between sources
- If RSS item description is empty, display title only and skip description segment.
- Cold boot now performs an immediate RSS/sports refresh in a background task while the display scrolls `Now Loading...`.
- After boot refresh finishes, scheduler transition waits for the current loading message to complete before switching.
- Web root now sends no-cache headers to reduce stale UI behavior after updating LittleFS content.
- Sports feed URL generation now uses JSON endpoint/query semantics (`espn_scores_json.php` + `format=json`).
- Feed fetcher parses sports JSON payloads (including nested scoreboard/event objects) and falls back to RSS parser when needed.
