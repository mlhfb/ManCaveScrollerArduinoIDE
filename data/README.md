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
