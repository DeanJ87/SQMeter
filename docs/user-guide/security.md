# Security

SQMeter is an observatory instrument designed to run on a trusted LAN. This page describes the security model, what HTTP auth protects, and recommended deployment.

---

## Threat model

SQMeter assumes the local network is trusted. The main risks on a typical home or observatory LAN are:

- **Unauthorised config changes** — any device on the same WiFi segment can call `POST /api/config` and change settings, including WiFi credentials or MQTT broker.
- **Unauthorised OTA updates** — any device on the LAN can flash new firmware or a new filesystem image via `POST /api/update`.
- **Credential exposure** — credentials sent over HTTP are visible to passive LAN observers. SQMeter does not support TLS.
- **Restart/reboot abuse** — `POST /api/restart` is unauthenticated by default; any LAN device can reboot the device.

SQMeter does **not** defend against:

- An attacker who already has LAN access and captures HTTP traffic (no TLS).
- Brute-force attacks; there is no rate limiting or account lockout.
- Remote internet attackers — do not expose the device to the public internet.

---

## HTTP authentication

HTTP Basic Auth can be enabled to require credentials on mutation endpoints. This is disabled by default.

### Protected endpoints (when auth is enabled)

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/config` | POST / PUT | Save configuration |
| `/api/restart` | POST | Reboot device |
| `/api/update` | POST | Flash firmware OTA |
| `/api/update/fs` | POST | Flash filesystem OTA |
| `/api/wifi/connect` | POST | Change WiFi network |
| `/api/mqtt/test` | POST | Test MQTT broker connection |

### Unprotected endpoints (always accessible)

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/sensors` | GET | Current sensor readings |
| `/api/status` | GET | System status |
| `/api/config` | GET | Read config (secrets masked) |
| `/api/wifi/scan` | GET | Scan WiFi networks |
| `/ws/sensors` | WS | Live sensor stream |
| `/ws/status` | WS | Live status stream |

Read-only integrations (Home Assistant, scripts polling sensor data) continue to work without credentials when auth is enabled.

### Setup

1. Open the Settings page in the web UI.
2. Under **HTTP Authentication**, enable the toggle.
3. Set a username (default `admin`) and a strong password.
4. Save configuration.

Once saved, the browser will prompt for credentials on the next mutation request. Most browsers remember credentials per origin for the session.

### Password masking and preservation

`GET /api/config` returns `********` for the auth password. When you POST config back, sending `********` or an empty string preserves the stored password unchanged. Send `null` only to intentionally clear the password.

### Password reset / lockout

If you forget the HTTP auth credentials:

1. Connect the device to a serial console (115200 baud).
2. Factory-reset via NVS clear: flash a sketch that calls `nvs_flash_erase()`, or use `esptool.py` to erase NVS.
3. The device will start with default config (no auth).

Alternatively, if you have physical access and can reflash, flash the complete image from the latest release.

---

## OTA security

Web OTA (`/api/update`, `/api/update/fs`) does not require authentication by default — any LAN device can push firmware. Enable HTTP auth to gate these endpoints.

Command-line ArduinoOTA is separate and controlled by the `ota.enabled` and `ota.password` config fields. It is disabled unless explicitly configured.

---

## Recommendations

| Scenario | Recommendation |
|----------|---------------|
| Home observatory, trusted LAN | Enable HTTP auth with a strong password |
| Shared network or observatory with multiple users | Enable HTTP auth; consider VLAN isolation |
| Public internet exposure | **Do not do this.** Place behind a VPN or firewall. |
| Sensitive automation (roof safety) | Validate sensor data independently; SQMeter is a monitor, not a safety controller |

---

## What HTTP auth does not protect

- Traffic confidentiality: credentials and sensor data travel in plaintext over HTTP. Anyone who can observe the LAN can capture them.
- Physical access: someone with physical access to the device can reset it.
- Denial of service: unauthenticated read endpoints remain open and could be flooded.
- Replay attacks: no session management or CSRF protection.
