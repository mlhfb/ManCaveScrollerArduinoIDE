This directory is for PlatformIO unit/integration tests.

Planned initial coverage:
- settings serialization/deserialization
- RSS text sanitization helpers
- cache selection and no-repeat cycle behavior

Current status:
- No automated tests implemented yet.
- Manual validation currently uses:
- `pio run` firmware build
- `pio run -t buildfs` LittleFS image build
- flash + serial control checks for brightness/speed, pixel-step toggle (`p`), and scheduler mode switching (`m`/`r`/`b`/`a`)
- AP/config mode and web API/UI smoke checks
- Mount regression check: confirm boot no longer reports missing `"spiffs"` partition after LittleFS label fix.
- Stability regression check: exit config mode and verify no `loopTask` stack overflow during RSS refresh.
- Smoothness regression check: outside config mode verify scrolling remains smooth with WiFi/web tasks suspended.
- Config-mode reachability check: entering config mode should keep web page reachable and scroller should show mode/SSID/IP.
- RSS save regression check: enable sports selections + base URL, save, reload status, verify fields persist.
- RSS playback mode check:
- random mode: verify non-repeat randomized source/item order
- ordered mode: verify source traversal in UI order (`mlb, nhl, ncaaf, nfl, nba, big10`) and in-order item playback.
