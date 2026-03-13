# OTA Backend How-To

This document describes what your server must provide for the ESP32 OTA client now implemented in this repo.

## 1) Device Expectations

The firmware OTA client performs:

1. `POST /api/ota/check` on device UI/API.
2. Device downloads manifest JSON from `manifest_url`.
3. Device compares `manifest.version` to `APP_FIRMWARE_VERSION`.
4. If newer, `POST /api/ota/update` downloads `firmware.url` and installs it.

Required manifest fields:

1. `version` (string)
2. `firmware.url` (string)

Optional but recommended fields:

1. `firmware.size` (integer bytes)
2. `firmware.md5` (string, 32 hex chars, lowercase preferred)
3. `firmware.sha256` (string)

## 2) Manifest Format

```json
{
  "version": "0.2.1",
  "build_date": "2026-03-12",
  "firmware": {
    "url": "https://charlie.servebeer.com/OTA/firmware-0.2.1.bin",
    "size": 1120608,
    "md5": "0123456789abcdef0123456789abcdef",
    "sha256": "optional_sha256_hex"
  },
  "notes": "OTA bug fixes and stability updates"
}
```

## 3) Hosting Requirements

1. Host manifest JSON and firmware `.bin` over HTTPS.
2. Serve manifest with `Cache-Control: no-cache, no-store, must-revalidate`.
3. Serve firmware with `Content-Length`.
4. Keep old firmware files available for rollback operations.

## 4) Release Workflow

1. Build firmware:
```powershell
pio run -e esp32doit-devkit-v1
```
2. Copy artifact from:
`./.pio/build/esp32doit-devkit-v1/firmware.bin`
3. Generate checksums:
```powershell
(Get-FileHash .\.pio\build\esp32doit-devkit-v1\firmware.bin -Algorithm MD5).Hash.ToLower()
(Get-FileHash .\.pio\build\esp32doit-devkit-v1\firmware.bin -Algorithm SHA256).Hash.ToLower()
```
4. Upload binary to your OTA file host.
5. Update `manifest.json` with new `version`, `url`, `size`, and checksum(s).
6. Deploy manifest with no-cache headers.

## 5) Example Directory Layout

```text
/var/www/updates/mancavescroller/
  manifest.json
  firmware-0.2.0.bin
  firmware-0.2.1.bin
```

## 6) Quick Validation

1. Confirm manifest is reachable:
```powershell
curl https://charlie.servebeer.com/OTA/manifest.json
```
2. Confirm firmware URL is reachable and content length is present:
```powershell
curl -I https://charlie.servebeer.com/OTA/firmware-0.2.1.bin
```
3. In device UI, set manifest URL and click `Check for OTA Update`.
4. If update is available, click `Install Available Update`.

## 7) Device Configuration

Set local `include/Secrets.h`:

```cpp
#define APP_OTA_MANIFEST_URL "https://charlie.servebeer.com/OTA/manifest.json"
```

You can override this URL from the web UI OTA panel at runtime.

## 8) Security Notes

1. Use HTTPS only.
2. Restrict who can publish to the OTA endpoint.
3. Consider adding signature verification (manifest signing) in a later hardening phase.
4. Do not reuse OTA hosting credentials on the device.

## 9) OTA Troubleshooting

1. `Update end failed: MD5 Check Failed`:
   - Ensure manifest MD5 matches the hosted binary bytes.
   - Re-download from firmware URL and hash that downloaded file.
2. `Failed to begin manifest request`:
   - Confirm URL includes host and is reachable from device network.
   - Device accepts bare host paths and auto-normalizes to `https://.../manifest.json`.
3. Device still serving old UI:
   - Re-run `pio run -t uploadfs` and hard-refresh browser.
