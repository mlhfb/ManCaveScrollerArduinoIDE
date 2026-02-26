# Changelog

All notable changes to this project are documented here.

## [Unreleased] - 2026-02-26

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
- Cold-boot startup refresh flow:
  - on boot (when WiFi creds + RSS sources are configured), runtime fetches fresh RSS/sports before normal playback
  - display scrolls `Now Loading...` while refresh runs
  - transition to normal playback waits for current loading scroll cycle to complete
- WiFi password visibility toggle button in web UI.
- Serial debug tracing for content scheduler starts and RSS source refresh/pick events.
- Serial `n` command to skip/advance to next scroll item immediately.

### Changed
- Scroll speed now adjusts only `FastLED.delay()` timing, with default `speed=10` mapping to `0 ms`.
- Pixel step is independent from speed and toggled via serial `p` (`1 -> 2 -> 3`), default `1`.
- Scheduler RSS mode now supports real cached RSS title/description playback instead of placeholder-only behavior.
- `/api/status` now includes `rss_source_count` and `rss_sources[]` cache metadata (`cache_valid`, `cache_item_count`, `cache_updated_epoch`).
- Serial controls now include manual/auto scheduler selection (`m`/`r`/`b` and `a`).
- Runtime mode behavior is now scroll-priority:
  - outside config mode: WiFi/web/RSS refresh work is suspended for max scroll smoothness
  - inside config mode: scrolling and WiFi/web run simultaneously
- Config-mode prompt text now includes current mode + SSID + IP address for discoverability.
- Added selectable RSS playback mode in config UI:
  - `Random item order` enabled: no-repeat random selection across enabled sources
  - `Random item order` disabled: deterministic source-order traversal with ordered item playback
- Ordered mode now refreshes each source at source-cycle start (when connected in config mode) before scrolling its items, matching rssArduinoPlatform-style behavior.
- Ordered mode now also refreshes each source cycle outside config mode by radio-cycling STA on/off between sources.
- UI label for random toggle is more explicit (`Shuffle RSS/sports items (random)`).
- Advanced UI playback-order section now explicitly exposes randomization toggle wording (`Randomize RSS/sports item order (shuffle)`).
- Advanced UI randomization toggle now defaults to OFF (unless persisted settings explicitly enable it).
- UI includes a build stamp (`UI build: 2026-02-26`) for cache/version verification.
- Sports source URL builder now targets backend JSON mode via `espn_scores_rss.php?sport=<sport>&format=json` and normalizes legacy `espn_scores_json.php` path text.
- Ordered (non-random) source playback now traverses selected sports in this order:
  `mlb, nhl, ncaaf, nfl, nba, big10`, then `npr`.

### Fixed
- Resolved lack of noticeable speed impact by widening delay profile and enforcing top speed delay `0 ms`.
- Corrected cache bitset helper conflict with Arduino macro namespace (`bitSet`).
- Fixed LittleFS mount failure on ESP32 Arduino by explicitly mounting partition label `"littlefs"` (default Arduino label is `"spiffs"`).
- Fixed `loopTask` stack overflow during RSS refresh by moving large fetch item buffer off stack to persistent runtime storage.
- Reduced repeated LittleFS open errors for missing cache files by checking file existence before opening.
- Removed per-frame cache filesystem checks from auto mode arbitration path to reduce scroll jitter.
- Fixed config-mode web reachability regression by preventing RSS refresh logic from cycling WiFi radio while config mode is active.
- Fixed RSS/sports settings reliability in web UI by preventing periodic status polling from overwriting unsaved form edits.
- Added compatibility in `/api/rss` handler for both compact and `rss_*` key naming variants and explicit save-failure response.
- Added handlers for common probe routes (`/favicon.ico`, `/generate_204`, `/hotspot-detect.html`, `/ncsi.txt`) to reduce spurious `request handler not found` errors.
- RSS item rendering now skips description segment when description is empty (title-only item display).
- Root UI responses now include explicit no-cache headers to reduce stale web UI behavior after `uploadfs`.
- Sports payload ingestion now supports JSON parsing for scoreboard-style objects/arrays with RSS parse fallback for compatibility.
- Web UI parity restored for WiFi password reveal/hide control.
- Backend sports JSON compatibility improved:
  - nested `home/away.score` values and `detail` text are now captured
  - sports entries now scroll as one complete combined message (matchup + score/start detail)
