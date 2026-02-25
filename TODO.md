# TODO

## 0) Ground Rules
- [x] Keep this repo on Arduino framework (`framework = arduino`) for `esp32doit-devkit-v1`.
- [x] Use `rssArduinoPlatform` only for platform/hardware/scroll reference, not for architecture or code style.
- [ ] Reach feature parity with `mlhfb/ManCaveScroller` behavior before adding new extras.

## 1) Project Bootstrap
- [x] Create `platformio.ini` for ESP32 DoIt DevKit V1 with Arduino framework and required libs.
- [x] Define partition strategy (app + LittleFS) and confirm `uploadfs` workflow.
- [x] Create `src/`, `include/`, `data/` (LittleFS assets), and `test/` structure.
- [ ] Pin compatible Arduino-core versions of async web stack and rotary encoder libs (deferred to Phase 3-6 integration).

## 2) Core Data Model + Persistence
- [ ] Define strongly typed settings structs/classes for:
- [ ] Messages (5 slots, text/color/enabled)
- [ ] Appearance (speed, brightness, panel width)
- [ ] WiFi credentials
- [ ] RSS global toggles and source manifest (NPR + sports)
- [ ] Implement settings load/save with schema versioning and safe defaults.
- [ ] Add migration path for future setting changes (no silent resets).

## 3) Display + Smooth Scroller
- [x] Implement LED panel abstraction for 8x32 chained panels (32/64/96/128 width), serpentine column mapping.
- [x] Mirror legacy `scrollMe()` behavior as baseline: start `xw = matrix->width()`, draw at cursor `xw`, decrement each frame, stop at `-(textLen * 6)`, then reset.
- [x] Implement Arduino/FastLED scroller update loop around that baseline (`setCursor` + `print` + `show`) tuned for smooth motion.
- [x] Support per-message RGB colors and runtime brightness changes.
- [x] Add runtime speed control profile with larger visual steps (`speed 10 => 0 ms` delay).
- [x] Add cycle-complete signaling so scheduler can switch items cleanly.

## 4) Runtime Scheduler
- [x] Build cooperative state machine in `loop()` (no long blocking loops).
- [x] Implement content arbitration:
- [x] Config mode text
- [x] RSS playback (title then description) (placeholder mode scaffolding)
- [x] Fallback custom messages when RSS unavailable
- [x] Keep deterministic source ordering and fair item rotation.

## 5) WiFi + Config Mode
- [ ] Implement AP bootstrap flow when no STA creds are available.
- [ ] Implement STA connection flow with timeout/retry.
- [ ] Implement BOOT button config mode toggle (enter: WiFi/web on, exit: apply settings and resume scrolling).
- [ ] Minimize WiFi-induced display artifacts (radio on only when needed in STA mode).
- [ ] Add captive portal DNS behavior for AP mode.

## 6) Web Server + API Contract
- [ ] Serve web UI from LittleFS.
- [ ] Implement endpoints matching current ManCaveScroller contract:
- [ ] `GET /api/status`
- [ ] `POST /api/messages`
- [ ] `POST /api/text` (legacy)
- [ ] `POST /api/color` (legacy)
- [ ] `POST /api/speed`
- [ ] `POST /api/brightness`
- [ ] `POST /api/appearance`
- [ ] `POST /api/wifi`
- [ ] `POST /api/advanced`
- [ ] `POST /api/rss`
- [ ] `POST /api/factory-reset`
- [ ] Validate payloads and return consistent JSON error messages.

## 7) RSS Fetch + Parse + Sanitize
- [ ] Implement HTTPS fetch pipeline with retry/backoff and timeouts.
- [ ] Parse RSS XML items (`title`, `description`) with strict bounds checks.
- [ ] Normalize content for LED display:
- [ ] Strip HTML tags / CDATA
- [ ] Decode entities
- [ ] Sanitize UTF-8 to display-safe ASCII fallback

## 8) RSS Cache + Playback Resilience
- [ ] Implement per-source cache files in LittleFS with header metadata.
- [ ] Preserve last good cache on fetch failures.
- [ ] Add no-repeat random picker across enabled cached sources until cycle exhaustion.
- [ ] Add item flags hook (`LIVE`) for future hot-list prioritization.
- [ ] Schedule periodic refresh and retry interval when feeds fail.

## 9) UI/UX Implementation
- [ ] Build lightweight web UI for:
- [ ] Message editing
- [ ] WiFi setup
- [ ] Appearance controls
- [ ] Advanced panel settings
- [ ] RSS + sports source configuration
- [ ] Factory reset confirmation
- [ ] Ensure UI and API schemas stay synchronized.

## 10) Validation + Release Readiness
- [ ] Add host-side tests for parsing/sanitization and settings serialization where practical.
- [ ] Run on-device smoke tests for:
- [ ] 32/64/96/128 panel widths
- [ ] AP/STA transitions
- [ ] Config mode toggle while scrolling
- [ ] RSS down/up recovery
- [ ] Document flash, upload, and troubleshooting workflow.
- [ ] Tag first implementation milestone as `v0.1.0` once end-to-end behavior is stable.
