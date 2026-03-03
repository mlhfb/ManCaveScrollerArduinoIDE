# OTA Update Draft (ESP32 4MB, Current Project)

## 1) Current State and Constraints

- Board flash: 4MB (`0x400000`)
- Current partition table: single app (`factory`, 2MB) + `littlefs` (~1.94MB)
- Current firmware size (recent build): ~1.10MB
- Result: firmware OTA is **not available** with current partitions because no `ota_0/ota_1` + `otadata` exist.

`LittleFS` is a separate data partition. It is not inside the app image.

## 2) Recommended Partition Layout (4MB, Safe A/B OTA)

This is the recommended baseline for your current firmware size.

```csv
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     0x9000,   0x5000,
otadata,   data, ota,     0xE000,   0x2000,
app0,      app,  ota_0,   0x10000,  0x180000,
app1,      app,  ota_1,   0x190000, 0x180000,
littlefs,  data, spiffs,  0x310000, 0x0F0000,
```

Capacity summary:

- `app0`: 1.5MB
- `app1`: 1.5MB
- `littlefs`: 960KB

Why this fits now:

- Current firmware (~1.10MB) leaves ~400KB headroom per slot.
- Current static data assets are tiny; RSS cache + UI can fit comfortably in 960KB.

## 3) Can OTA Slot Double as LittleFS?

No. An `app` partition (OTA firmware) cannot simultaneously act as a mounted LittleFS partition.

You can back up data before OTA and restore after OTA, but that backup target must be cloud or another dedicated data partition.

## 4) Boot-Time OTA UX (WiFi Off While Scrolling Policy)

Policy target:

1. Boot.
2. Temporarily enable WiFi.
3. Check update manifest.
4. If update exists, show prompt in web UI (and optional button flow).
5. On user confirm, perform OTA.
6. Reboot into new slot.
7. Disable WiFi during normal scrolling runtime.

This preserves your "WiFi only when needed" behavior.

## 5) Cloud Backup/Restore Flow (LittleFS-Aware)

Recommended data classification:

- Critical and small: settings (keep in NVS/SettingsStore as source of truth)
- Rebuildable: RSS cache files, transient artifacts
- Optional backup: UI custom files if user-editable in future

Flow:

1. `backup-start`:
   - Enumerate allowed LittleFS files.
   - Upload JSON/settings and selected files to cloud object store.
   - Return backup ID + checksum.
2. `ota-start`:
   - Download firmware to inactive app slot.
   - Verify SHA-256.
   - Set boot partition and reboot.
3. `restore-start` (post-boot):
   - Use backup ID from NVS marker.
   - Re-download selected files.
   - Validate checksums.
4. `restore-complete`:
   - Clear marker, report success in `/api/ota/status`.

Important: if backup/restore fails, device should still boot new firmware and regenerate rebuildable cache.

## 6) OTA Manifest Draft

```json
{
  "version": "0.2.0",
  "build_date": "2026-03-02",
  "firmware": {
    "url": "https://example.com/releases/mancavescroller-0.2.0.bin",
    "size": 1234567,
    "sha256": "hexstring..."
  },
  "min_loader_version": "0.0.0",
  "notes": "Bug fixes and OTA support"
}
```

Validation rules:

- `size` <= inactive OTA slot size
- `sha256` must match downloaded binary
- semantic version must be newer than current

## 7) Local API Draft

Add endpoints to existing web server:

- `GET /api/ota/status`
  - state (`idle/checking/available/downloading/verifying/installing/rebooting/error`)
  - current version
  - available version
  - progress percent
  - last error
- `POST /api/ota/check`
  - forces manifest check
- `POST /api/ota/update`
  - body: `{ "confirm": true, "backup": true }`
  - starts backup + OTA sequence
- `POST /api/ota/restore`
  - optional manual restore retry

## 8) Failure and Recovery Rules

- Download failure: remain on current firmware, report error.
- Verify failure (hash/size): abort OTA, keep current firmware.
- Power loss during OTA write: boot remains on previous valid slot.
- New firmware boot failure: rely on ESP32 OTA rollback where applicable; additionally expose manual rollback endpoint if desired.
- Backup/restore failure: continue operation, regenerate non-critical data.

## 9) Migration Plan (One-Time USB Flash Required)

1. Create new partition CSV (do not overwrite current until ready).
2. Update `platformio.ini` to point to OTA partition CSV.
3. USB flash firmware + new partitions once.
4. Run `uploadfs` once for resized LittleFS.
5. Verify:
   - app boots
   - LittleFS mounts with new size
   - `/api/status` works
   - OTA check endpoint works (dry run)

After this one-time migration, regular firmware updates can be OTA.

## 10) If Firmware Grows Beyond 1.5MB

Options:

1. Optimize firmware to stay within 1.5MB slots (preferred on 4MB modules).
2. Use loader architecture:
   - small `factory` loader + one large app slot + smaller LittleFS
   - loses true A/B safety
3. Move to 8MB ESP32 modules (best long-term if firmware growth is expected).

For this project today, Option 1 (A/B OTA with 1.5MB slots) is the best tradeoff.
