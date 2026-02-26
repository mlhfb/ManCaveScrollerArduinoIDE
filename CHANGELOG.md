# Changelog

All notable changes to this project are documented here.

## [Unreleased] - 2026-02-25

### Added
- Project planning docs and phased execution docs (`README.md`, `TODO.md`, `CLAUDE.md`).
- Arduino PlatformIO scaffold for `esp32doit-devkit-v1` with LittleFS partitioning and uploadfs workflow.
- Core display/scroller stack:
  - `DisplayPanel` for FastLED/FastLED_NeoMatrix panel control
  - `Scroller` with legacy `scrollMe()`-style motion bounds
  - `ContentScheduler` cooperative runtime arbitration
- Persistent settings store in LittleFS (`/config/settings.json`) with schema/defaults and fallback default messages from `/config/default_messages.json`.
- WiFi/config runtime services:
  - AP bootstrap when no STA credentials are present
  - STA timeout/retry connection path
  - BOOT-button config mode toggle
  - AP captive DNS support
- Full web API contract implementation:
  - `GET /api/status`
  - `POST /api/messages`
  - `POST /api/text`
  - `POST /api/color`
  - `POST /api/speed`
  - `POST /api/brightness`
  - `POST /api/appearance`
  - `POST /api/wifi`
  - `POST /api/advanced`
  - `POST /api/rss`
  - `POST /api/factory-reset`
- Full LittleFS web setup UI (`/web/index.html`) for messages, appearance, WiFi, advanced panel settings, RSS/sports config, and factory reset.
- RSS pipeline modules:
  - `RssSources` for deterministic source manifest generation (NPR + sports)
  - `RssSanitizer` for CDATA/tag/entity/UTF-8 cleanup
  - `RssFetcher` for HTTPS feed retrieval with retry/backoff
  - `RssCache` for per-source cache files + metadata
  - `RssRuntime` for periodic refresh scheduling and cached playback

### Changed
- Scroll speed now adjusts only `FastLED.delay()` timing, with default `speed=10` mapping to `0 ms`.
- Pixel step is independent from speed and toggled via serial `p` (`1 -> 2 -> 3`), default `1`.
- Scheduler RSS mode now supports real cached RSS title/description playback instead of placeholder-only behavior.
- `/api/status` now includes `rss_source_count` and `rss_sources[]` cache metadata (`cache_valid`, `cache_item_count`, `cache_updated_epoch`).
- Serial controls now include manual/auto scheduler selection (`m`/`r`/`b` and `a`).

### Fixed
- Resolved lack of noticeable speed impact by widening delay profile and enforcing top speed delay `0 ms`.
- Corrected cache bitset helper conflict with Arduino macro namespace (`bitSet`).
- Fixed LittleFS mount failure on ESP32 Arduino by explicitly mounting partition label `"littlefs"` (default Arduino label is `"spiffs"`).
