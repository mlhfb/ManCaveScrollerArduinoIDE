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
