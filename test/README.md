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
- Ordered-mode radio cycle check: outside config mode verify WiFi reconnect/fetch between source cycles and radio-off scrolling between fetches.
- Empty description check: confirm title-only display when RSS item has no description.
- Cold-boot refresh check: with WiFi+RSS configured, verify startup scroll shows `Now Loading...` until fresh fetch completes.
- Cold-boot transition check: verify runtime waits for loading message cycle completion before switching to normal playback.
- UI parity check: verify Advanced page shows playback randomization toggle and `UI build: 2026-02-26` stamp after `uploadfs`.
- Random default check: on fresh settings/factory reset verify playback randomization checkbox starts unchecked.
- Sports format check: verify sports feed requests use backend JSON mode (`espn_scores_rss.php` + `format=json`) and parsed items scroll.
- Sports message completeness check: verify each sports scroll includes matchup plus score/start detail in one single segment.
- Future-game formatting check: verify scheduled games show `Team at Team  <start detail>` without `0 at 0`.
- Score-color check: verify higher score renders green and lower score renders red on scored games.
- Ordered sequence check: with random disabled verify selected sports scroll in `mlb, nhl, ncaaf, nfl, nba, big10` order, then `npr`.
- UI check: verify WiFi password field has show/hide button behavior.
- UI save/exit check: click `Save + Exit Config Mode`, verify settings persist and device leaves config mode.
- Serial control check: press `n` repeatedly and confirm immediate item advancement across current mode.
- Debug log check: verify serial prints source refresh/pick lines and `[SCROLL]` lines for each started text segment.
- Input check: GPIO35 encoder button press (active-low with external pull-up) toggles config mode like BOOT.
- Time format check: interstitial time should be Eastern and render as `THU FEB 26 -- 15:48` style.
- Secrets check: with missing `include/Secrets.h`, verify weather log reports missing `APP_WEATHER_API_URL` and scroll fallback is `Weather unavailable`.
