# TODO

## 0) Ground Rules
- [x] Keep this repo on Arduino framework (`framework = arduino`) for `esp32doit-devkit-v1`.
- [x] Use `rssArduinoPlatform` only for platform/hardware/scroll reference, not for architecture or code style.
- [x] Reach feature parity with `mlhfb/ManCaveScroller` behavior before adding new extras.

## 1) Project Bootstrap
- [x] Create `platformio.ini` for ESP32 DoIt DevKit V1 with Arduino framework and required libs.
- [x] Define partition strategy (app + LittleFS) and confirm `uploadfs` workflow.
- [x] Create `src/`, `include/`, `data/` (LittleFS assets), and `test/` structure.
- [x] Pin compatible Arduino-core dependency set for current implementation (`WebServer` + `DNSServer` + `WiFi` + `ArduinoJson` + `FastLED`); async stack and rotary libs not required in this architecture.

## 2) Core Data Model + Persistence
- [x] Define strongly typed settings structs/classes for:
- [x] Messages (5 slots, text/color/enabled)
- [x] Appearance (speed, brightness, panel width)
- [x] WiFi credentials
- [x] RSS global toggles and source manifest (NPR + sports)
- [x] Implement settings load/save with schema versioning and safe defaults.
- [x] Add migration path for future setting changes (no silent resets).

## 3) Display + Smooth Scroller
- [x] Implement LED panel abstraction for 8x32 chained panels (32/64/96/128 width), serpentine column mapping.
- [x] Mirror legacy `scrollMe()` behavior as baseline: start `xw = matrix->width()`, draw at cursor `xw`, decrement each frame, stop at `-(textLen * 6)`, then reset.
- [x] Implement Arduino/FastLED scroller update loop around that baseline (`setCursor` + `print` + `show`) tuned for smooth motion.
- [x] Support per-message RGB colors and runtime brightness changes.
- [x] Add runtime speed control via `FastLED.delay()` (`speed 10 => 0 ms` delay) and independent pixel-step toggle (`1/2/3`).
- [x] Add cycle-complete signaling so scheduler can switch items cleanly.

## 4) Runtime Scheduler
- [x] Build cooperative state machine in `loop()` (no long blocking loops).
- [x] Implement content arbitration:
- [x] Config mode text
- [x] RSS playback (title then description) (placeholder mode scaffolding)
- [x] Fallback custom messages when RSS unavailable
- [x] Keep deterministic source ordering and fair item rotation.

## 5) WiFi + Config Mode
- [x] Implement AP bootstrap flow when no STA creds are available.
- [x] Implement STA connection flow with timeout/retry.
- [x] Implement BOOT button config mode toggle (enter: WiFi/web on, exit: apply settings and resume scrolling).
- [x] Implement external encoder button (GPIO35, active-low) to mirror BOOT config-mode toggle behavior.
- [x] Minimize WiFi-induced display artifacts (radio on only when needed in STA mode).
- [x] Add captive portal DNS behavior for AP mode.

## 6) Web Server + API Contract
- [x] Serve web UI from LittleFS.
- [x] Implement endpoints matching current ManCaveScroller contract:
- [x] `GET /api/status`
- [x] `POST /api/messages`
- [x] `POST /api/text` (legacy)
- [x] `POST /api/color` (legacy)
- [x] `POST /api/speed`
- [x] `POST /api/brightness`
- [x] `POST /api/appearance`
- [x] `POST /api/wifi`
- [x] `POST /api/advanced`
- [x] `POST /api/rss`
- [x] `POST /api/exit-config`
- [x] `POST /api/factory-reset`
- [x] Validate payloads and return consistent JSON error messages.

## 7) RSS Fetch + Parse + Sanitize
- [x] Implement HTTPS fetch pipeline with retry/backoff and timeouts.
- [x] Parse RSS XML items (`title`, `description`) with strict bounds checks.
- [x] Normalize content for LED display:
- [x] Strip HTML tags / CDATA
- [x] Decode entities
- [x] Sanitize UTF-8 to display-safe ASCII fallback

## 8) RSS Cache + Playback Resilience
- [x] Implement per-source cache files in LittleFS with header metadata.
- [x] Preserve last good cache on fetch failures.
- [x] Add no-repeat random picker across enabled cached sources until cycle exhaustion.
- [x] Add item flags hook (`LIVE`) for future hot-list prioritization.
- [x] Schedule periodic refresh and retry interval when feeds fail.
- [x] On cold boot with configured RSS + WiFi, fetch fresh RSS/sports content before normal playback while scrolling a loading prompt.

## 9) UI/UX Implementation
- [x] Build lightweight web UI for:
- [x] Message editing
- [x] WiFi setup
- [x] Appearance controls
- [x] Advanced panel settings
- [x] RSS + sports source configuration
- [x] Factory reset confirmation
- [x] Ensure UI and API schemas stay synchronized.

## 10) Validation + Release Readiness
- [ ] Add host-side tests for parsing/sanitization and settings serialization where practical.
- [ ] Run on-device smoke tests for:
- [ ] 32/64/96/128 panel widths
- [ ] AP/STA transitions
- [ ] Config mode toggle while scrolling
- [ ] RSS down/up recovery
- [x] Document flash, upload, and troubleshooting workflow.
- [ ] Tag first implementation milestone as `v0.1.0` once end-to-end behavior is stable.

## Notes
- 2026-02-26: Fixed LittleFS mount regression by mounting partition label `littlefs` explicitly in `SettingsStore`.
- 2026-02-26: Fixed RSS refresh `loopTask` stack overflow by removing large on-stack item buffer from runtime refresh path.
- 2026-02-26: Reduced missing-cache file spam by checking cache file existence before open.
- 2026-02-26: Optimized runtime for smoother scrolling by suspending WiFi/web/RSS refresh outside config mode; config mode still runs scrolling + WiFi simultaneously.
- 2026-02-26: Config-mode prompt now displays mode, SSID, and IP; RSS runtime no longer cycles WiFi radio during config mode.
- 2026-02-26: Fixed sports/RSS save UX issue by stopping background status polling from overwriting unsaved form edits.
- 2026-02-26: Added API probe/fallback routes and `/api/rss` key-compatibility handling.
- 2026-02-26: Added selectable RSS playback mode (`random` vs `ordered`) in UI/API/settings.
- 2026-02-26: Ordered mode now traverses configured source order and refreshes each source before its item cycle both in config mode and in non-config mode (radio on/off per source cycle).
- 2026-02-26: RSS items with empty descriptions now display title only.
- 2026-02-26: Cold boot now starts a background refresh task; display scrolls `Now Loading...` until refresh completes, then transitions on cycle boundary.
- 2026-02-26: Web UI advanced section now labels playback order more explicitly and includes UI build stamp for cache/version checks.
- 2026-02-26: Web root now sends no-cache headers to reduce stale UI asset behavior.
- 2026-02-26: Random playback default changed to OFF for new/default settings and UI fallback state.
- 2026-02-26: Sports source URL generation now targets backend JSON mode (`espn_scores_rss.php?format=json`).
- 2026-02-26: Fetch runtime now supports JSON feed parsing for sports with RSS fallback compatibility.
- 2026-02-26: Added Advanced UI password visibility toggle button for WiFi password field.
- 2026-02-26: Ordered playback source sequence now follows selected sports first (`mlb, nhl, ncaaf, nfl, nba, big10`) then `npr`.
- 2026-02-26: Added serial debug traces for selected source refreshes/picks and each text segment started by the scheduler.
- 2026-02-26: Added serial `n` shortcut to force-advance immediately to the next scroll item.
- 2026-02-26: Sports JSON parser now reads nested `home/away.score` and `detail` fields from backend payloads.
- 2026-02-26: Sports items now scroll as a single combined segment (`title | description`) instead of split title/description passes.
- 2026-02-26: Scheduled/future games now suppress score display (example style: `Team at Team  Thu, ...`).
- 2026-02-26: Live/final games now colorize winning score green and losing score red.
- 2026-02-26: RSS base color now rotates by item; renderer now supports inline per-character color markup.
- 2026-02-26: Weather API URL moved to local `include/Secrets.h` (`APP_WEATHER_API_URL`) with committed template `include/Secrets.example.h`.
- 2026-02-26: Added web `Save + Exit Config Mode` flow and `POST /api/exit-config` route.
- 2026-02-26: Added GPIO35 encoder button config-mode toggle; active-low behavior aligned with rssArduinoPlatform.
- 2026-02-26: Fixed startup sequencing so weather is re-queued first after boot loading completes.
- 2026-02-26: Time display now uses Eastern timezone with 24-hour uppercase format (`THU FEB 26 -- 15:48`).
- 2026-02-26: Sports team names now use backend JSON `teamColor` values while preserving winner/loser score colors and rotating base color for untagged text.
