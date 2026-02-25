# LittleFS Data Directory

Files in this folder are uploaded to the ESP32 LittleFS partition via:

`pio run -t uploadfs`

Planned content:
- `web/index.html` and related UI assets
- `config/default_messages.json`
- optional font or cache seed assets

Documentation note:
- Refreshed with current runtime defaults: scroll delay boots at `0 ms`, pixel step boots at `1`.
