# Contract: backup file (format 1)

A backup is UTF-8 JSON. The device sends it compact; the web UI saves it pretty-printed as
`sqmeter-<device name>-<YYYY-MM-DD>.json`. JSON Schema:
`specs/016-demo-device-emulation/contracts/schemas/backup.schema.json`.

```json
{
  "sqmeterBackup": 1,
  "firmware": "0.3.4-beta.1",
  "board": "esp32",
  "build": "standard",
  "deviceName": "backyard-sqm",
  "createdAt": "2026-10-18T21:04:05Z",
  "settings": { "deviceName": "backyard-sqm", "wifi": { "ssid": "Observatory", "password": "********" }, "…": "…" },
  "secrets": {
    "kdf": "pbkdf2-sha256",
    "iterations": 20000,
    "salt": "q83vEjRWeJq83vEjRWeJqw==",
    "cipher": "aes-256-gcm",
    "nonce": "AAECAwQFBgcICQoL",
    "data": "…base64 ciphertext + 16-byte tag…"
  }
}
```

- `settings` is `GET /api/config` at the time of the backup: every section including `alerts`; a set secret
  is `"********"`, an empty one `""`. Editing it by hand is allowed; the restore validates it like a save.
- `secrets` is present only when a passphrase was given. Key = PBKDF2-HMAC-SHA256(passphrase as UTF-8,
  salt, iterations, 32 bytes). `data` = AES-256-GCM(key, nonce, plaintext, associated data
  `sqmeter-backup/1`) as ciphertext ‖ tag. Plaintext:

  ```json
  {"wifi.password":"…","mqtt.password":"","ota.password":"","auth.password":"…","ble.passkey":"",
   "alerts.pushover.userKey":"","alerts.pushover.appToken":"","alerts.ntfy.token":"","alerts.webhook.authHeader":""}
  ```

  Unknown paths in the plaintext are ignored; a missing path keeps this device's value.
- `createdAt` is omitted when the device clock wasn't set.

## Reading rules

| File | Result |
|---|---|
| Not JSON, or JSON without `sqmeterBackup` and without a `wifi` object | `not-backup` |
| Text containing `"sqmeterBackup"` that doesn't parse | `damaged` |
| `sqmeterBackup` not a positive integer; `settings` missing or not an object; `secrets` with an unknown `kdf`/`cipher`, iterations outside 1,000-100,000, salt not 16 bytes, nonce not 12 bytes, data shorter than the tag, or bad base64 | `damaged` |
| `sqmeterBackup` > 1 | `newer-format` |
| A plain settings object (has a `wifi` object, no `sqmeterBackup`) | format 0: settings only, secrets kept |
| Tag doesn't verify (wrong passphrase or changed ciphertext) | `wrong-passphrase` |
| Passphrase given, file has no `secrets` | the passphrase is ignored |
| Over 20,480 bytes (device) / 64 KB (web UI) | `too-large`, before parsing |
