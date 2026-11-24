# Contract: shared passwords

## `GET /api/secrets`

Returns four stored secrets for a paired device (RoofRoof). Checked in this order:

| # | Condition | Status | Body |
|---|---|---|---|
| 1 | `sharing.secrets` is off | 403 | `{"error":"Password sharing is off in SQMeter"}` |
| 2 | Password protection off, or no password | 403 | `{"error":"Set a web password in SQMeter before sharing passwords"}` |
| 3 | Five wrong logins in a row (Basic credentials sent but wrong; bare requests don't count; 10 quiet minutes reset the count), the last under a minute ago | 429 | `{"error":"Too many failed logins; try again in a minute"}`, header `Retry-After: <seconds>` |
| 4 | Missing or wrong HTTP Basic login | 401 | the device's standard challenge (`WWW-Authenticate: Basic realm="SQMeter"`) |
| 5 | Otherwise | 200 | see below |

200 body, every key always present, each value the stored secret or `""` when not set:

```json
{"mqtt":{"password":"…"},"alerts":{"ntfy":{"token":"…"},"pushover":{"userKey":"…","appToken":"…"}}}
```

Nothing else is ever included (no WiFi, device, upload or webhook secret, no Bluetooth passkey).
Answers 1, 2, 3 and 5 carry `Cache-Control: no-store` and `Content-Type: application/json`. The
values are never logged. Plain HTTP on port 80: SQMeter has no TLS.

## Setting `sharing.secrets`

`GET /api/config` and `POST /api/config`: `{"sharing":{"secrets":false}}` (boolean, default `false`).
Older settings without it load as `false`. Kept by backup and restore.

## Settings dependency D-53

`sharing.secrets` depends on a device password (`auth.enabled` and a password set). Unmet: reported
`inactive` by `GET /api/settings/effective` with reason `no-web-password` ("Set a device password",
fix `device#security`); the UI disables the switch with the reason and the link.
