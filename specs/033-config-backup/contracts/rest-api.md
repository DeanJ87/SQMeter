# Contract: REST API

All three routes need the device password when protection is on (`requireAuth`, constitution VI) and are
registered before any shorter `/api/backup` prefix. Bodies are JSON (`Content-Type: application/json`),
at most 20,480 bytes (413 `too-large` above that, before parsing). Errors:
`{"error": "<English>", "code": "<code>"}` (data-model.md, Errors).

## `POST /api/backup`

Body (optional): `{"passphrase": "<8+ characters>"}`. Empty body or no passphrase: no secrets.

- 200: the backup file ([backup-file.md](backup-file.md)). Answered from a background task (key
  derivation 1-2 s). The web UI names the download `sqmeter-<name>-<date>.json`.
- 400 `passphrase-short`: fewer than 8 characters.
- 409 `busy`: another backup or preview is running.
- 413 `too-large`: body over 20,480 bytes.
- 503: no memory to start the task.

## `POST /api/backup/preview`

Body: `{"backup": <the backup file's JSON>, "passphrase": "<optional>"}`.

Nothing is applied. Answered from the background task.

- 200: the preview:

```json
{
  "token": "4f1c0d7e9a2b3c4d5e6f708192a3b4c5",
  "backup": { "format": 1, "firmware": "0.3.4-beta.1", "board": "esp32", "build": "ble", "deviceName": "backyard-sqm", "createdAt": "2026-10-18T21:04:05Z" },
  "settings": { "deviceName": "backyard-sqm", "wifi": { "ssid": "Observatory", "password": "********" }, "…": "…" },
  "secrets": { "state": "restored", "changed": ["wifi.password"], "unchanged": ["mqtt.password", "ota.password", "auth.password", "ble.passkey", "alerts.pushover.userKey", "alerts.pushover.appToken", "alerts.ntfy.token", "alerts.webhook.authHeader"] },
  "notApplicable": [ { "path": "gps.rxPin", "value": 44, "reason": "pin" } ],
  "restart": ["wifi"]
}
```

  `settings` is the complete candidate, masked like `GET /api/config`. The web UI lists the
  per-setting changes, the kept and ignored settings and the warnings from it (research R10).

- 400 `not-backup`, `damaged`, `newer-format`, `wrong-passphrase`, or `invalid` (the save's validation
  message in `error`, e.g. `"MQTT broker: …"`): nothing staged.
- 409 `busy`.

A new preview replaces any earlier one.

## `POST /api/backup/apply`

Body: `{"token": "<from the preview>"}`.

- 200 `{"success": true, "restarting": <bool>}`: the previewed settings were saved as one save (open pages
  reload them through `configRevision`). With `restarting`, the device restarts about 2 s later.
- 409 `stale`: no preview with that token (applied, replaced, expired after 10 minutes) or the settings
  changed since the preview. Preview again.
- 500: saving failed; nothing changed.

There is no cancel call: an unapplied preview changes nothing and expires.

## Demo

The demo answers the same three routes from its emulated device (`EmulatedDevice::backup`,
`restorePreview`, `restoreApply`), with the same documents.
