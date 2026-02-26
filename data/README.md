# LittleFS Data Directory

Files in this folder are uploaded to the ESP32 LittleFS partition via:

`pio run -t uploadfs`

Planned content:
- `web/index.html` and related UI assets
- `config/default_messages.json`
- optional font or cache seed assets

Current content:
- `/web/index.html`: full setup UI (messages, appearance, WiFi, advanced/RSS, factory reset)
- `/config/default_messages.json`: startup defaults for 5 message slots

Documentation note:
- Refreshed with current runtime defaults: scroll delay boots at `0 ms`, pixel step boots at `1`.
- Runtime mounts LittleFS using partition label `littlefs` (matching `partitions.csv`).
- Runtime cache reads now pre-check file existence to avoid repeated missing-file open errors.
- Runtime policy: outside config mode the firmware prioritizes scrolling and suspends WiFi/web refresh work.
- In config mode the scroller prompt includes mode/SSID/IP so users can reach the hosted page.
- Web UI polling refreshes status without overwriting unsaved config form values.
- Advanced UI includes RSS playback randomization toggle under `Playback Order`.
- Randomization toggle is OFF by default (`rss_random_enabled=false`) on fresh/default settings.
- WiFi form includes a password visibility toggle button.
- Current page stamp: `UI build: 2026-02-26`.
- If browser still shows an older UI (missing playback toggle), re-run `pio run -t uploadfs` and hard-refresh browser cache.
- Sports backend URL generation now targets `espn_scores_rss.php` with `format=json`.
- Sports messages now render as a single combined line per item (matchup + score/start detail).
- Scheduled sports games now render matchup + start detail without placeholder `0 at 0` scores.
