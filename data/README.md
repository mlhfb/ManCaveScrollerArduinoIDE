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
- Advanced UI now includes RSS playback mode toggle (`Random item order`).
- Random toggle label clarified in UI and ordered mode semantics documented in-page.
