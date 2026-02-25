# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- Initial planning baseline created on 2026-02-25.
- Added project planning docs:
- `README.md` with rewrite scope, references, target architecture, and milestones.
- `TODO.md` with phased implementation checklist.
- `CLAUDE.md` with engineering constraints and compatibility targets.
- Added Phase 1 scaffold:
- `platformio.ini` for ESP32 Arduino target and initial dependencies.
- `partitions.csv` with 2 MB app + ~1.94 MB LittleFS layout.
- Base project structure: `src/`, `include/`, `data/`, `test/`.
- Bootable `src/main.cpp` placeholder and initial LittleFS placeholder assets.
- Added Phase 2 display/scroller core:
- `DisplayPanel` abstraction over FastLED + FastLED_NeoMatrix.
- `Scroller` module with legacy `scrollMe()`-style horizontal step behavior and cycle-complete flag.
- Boot-time one-shot test message scroll in `src/main.cpp` for flash verification.
- Added next-phase message runtime behavior:
- Explicit built-in Adafruit GFX 5x7 font selection to match `rssArduinoPlatform`.
- Multi-message rotation with per-message RGB colors.
- Serial runtime brightness controls (`u` brighter, `d` dimmer, `h` help).
- Added Phase 4 scheduler scaffolding:
- New `ContentScheduler` module with cooperative non-blocking arbitration modes.
- Mode support wired via serial commands: `m` (messages), `c` (config text), `r` (RSS placeholder title/description), `x` (fallback).
- Serial runtime speed controls added: `f`/`s` for faster/slower and `1..9` / `0` for exact speed levels (1..10).
- Reworked runtime speed behavior for visible effect:
- Larger delay profile steps and per-speed pixel-step profile.
- Fastest speed (`10`) now uses `0 ms` delay.
- Scroller timing now uses `FastLED.delay()` in tick cadence path.
- Updated runtime control model:
- Speed now adjusts only `FastLED.delay()` timing.
- Pixel step is now independent and toggled with serial command `p` (`1 -> 2 -> 3`).
- Boot defaults are now `delay=0 ms` and `pixel step=1`.

### Notes
- Core firmware scaffolding is implemented through Phase 4 (display, scroller, scheduler placeholders, serial controls).
- Async web server and rotary encoder dependency pinning is intentionally deferred to later phases due Arduino-core compatibility/package installer issues observed during bootstrap.
- Documentation set (`*.md`) was refreshed with this push as requested.
