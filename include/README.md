# Include Directory

Current module headers:
- `AppConfig.h` - hardware and runtime speed profile constants
- `DisplayPanel.h` - matrix panel abstraction
- `Scroller.h` - legacy-style scrolling engine
- `ContentScheduler.h` - non-blocking content arbitration modes

Runtime defaults are defined in `AppConfig.h`:
- speed default maps to `0 ms` delay
- pixel step default is `1`
