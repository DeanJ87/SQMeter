# Data model: Settings backup and restore

## Backup file

JSON text, UTF-8. Full contract: [contracts/backup-file.md](contracts/backup-file.md).

| Field | Type | Rules |
|---|---|---|
| `sqmeterBackup` | integer | Format version. 1 for this release. Absent: the file is read as a format-0 plain settings export. Larger than 1: refused (`newer-format`). |
| `firmware` | string | Firmware version that made it (`FIRMWARE_VERSION`). |
| `board` | string | Chip/board (`esp32`). |
| `build` | `standard` \| `ble` | Firmware build. |
| `deviceName` | string | The device's name when backed up. |
| `createdAt` | string (ISO 8601 UTC) | Omitted when the device clock wasn't set. |
| `settings` | object | `GET /api/config` shape: every section, secrets as `"********"` (set) or `""` (empty). Required. |
| `secrets` | object | Present only when backed up with a passphrase. |
| `secrets.kdf` | `pbkdf2-sha256` | |
| `secrets.iterations` | integer | 1,000-100,000; written as 20,000. |
| `secrets.salt` | base64 | 16 bytes. |
| `secrets.cipher` | `aes-256-gcm` | |
| `secrets.nonce` | base64 | 12 bytes. |
| `secrets.data` | base64 | Ciphertext followed by the 16-byte tag. Plaintext: a JSON object of the nine secret paths to their values (`""` for empty). Associated data: `sqmeter-backup/1`. |

## Secret paths

`wifi.password`, `mqtt.password`, `ota.password`, `auth.password`, `ble.passkey`, `alerts.pushover.userKey`,
`alerts.pushover.appToken`, `alerts.ntfy.token`, `alerts.webhook.authHeader`.

## Board pins (`Backup::BoardPins`)

| Field | Meaning |
|---|---|
| `validPins` | Bit mask of GPIOs usable for sensor buses on this board. |
| `analogPins` | Bit mask of ADC1 pins (the wind vane). |

Pin settings checked: `gps.rxPin`, `gps.txPin`, `rain.rxPin`, `rain.txPin`, `sensor.i2cSDA`, `sensor.i2cSCL`,
`wind.speedPin` (valid), `wind.directionPin` (analog).

## Device identity (`Backup::DeviceInfo`)

`firmware`, `board`, `bluetoothBuild`, `pins` (BoardPins).

## Restore preview

Full contract: [contracts/rest-api.md](contracts/rest-api.md#post-apibackuppreview).

From the device:

| Field | Meaning |
|---|---|
| `token` | 32 hex characters; binds this preview to `POST /api/backup/apply`. |
| `backup` | The file's header: `format`, `firmware`, `board`, `build`, `deviceName`, `createdAt` (each present when known). |
| `settings` | The candidate config, masked as in `GET /api/config`. |
| `secrets` | `{state, changed[], unchanged[]}`. `state`: `restored` (passphrase right), `kept` (file has secrets, no passphrase given), `none` (file has none). Paths only; never values. |
| `notApplicable[]` | `{path, value?, reason}`; reason `pin`, `analog`, `bluetooth` or `pushover-keys`. |
| `restart[]` | Codes: `wifi`, `ota`, `gps`, `i2c`, `ipv6`, `alpaca`, `ble`. Empty: no restart. |

Worked out by the web UI (`web/src/lib/backupPreview.ts`, research R10):

| Item | From |
|---|---|
| Changes `{path, from, to}` | `GET /api/config` vs `settings` (secrets and `alerts.armWithAlpaca` left out). |
| Kept | Paths in `GET /api/config` missing from the file's settings (a whole missing section is one path). |
| Ignored | Paths in the file's settings missing from `settings`. |
| Warnings | `wifi-network` (`ssid`), `device-password`, `address` (`hostname`), `imaging-app`, `newer-firmware` (`firmware`). |

Paths and codes are API values, never translated (I18N-06); the web UI words them.

## Staged restore (firmware, in RAM)

| Field | Meaning |
|---|---|
| `token` | As returned. |
| `preview` | `Backup::Preview`: the complete validated candidate `Config` to save, and its restart bits. |
| `revision` | `configRevision` when previewed. |
| `stagedAtMs` | Expires after 10 minutes. |

State: none → staged (preview) → none (apply, expiry, or replaced by a new preview).

## Errors

`{"error": "<English message>", "code": "<code>"}` with code `too-large` (413), `not-backup`, `damaged`,
`newer-format`, `wrong-passphrase`, `passphrase-short`, `bad-request`, `invalid` (the save's own
validation message), `stale` (409), `busy` (409).
