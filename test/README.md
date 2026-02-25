This directory is for PlatformIO unit/integration tests.

Planned initial coverage:
- settings serialization/deserialization
- RSS text sanitization helpers
- cache selection and no-repeat cycle behavior

Current status:
- No automated tests implemented yet.
- Manual validation currently uses flash + serial control checks for brightness/speed, pixel-step toggle (`p`), and scheduler mode switching.
