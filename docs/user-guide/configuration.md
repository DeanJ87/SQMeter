# Configuration

All settings are stored in NVS (Non-Volatile Storage) and survive firmware and filesystem updates. Configure via the web UI Settings page or the REST API.

---

## Full Configuration Reference

```json
{
  "deviceName": "SQM-ESP32",
  "timezone": "UTC",
  "primaryTimeSource": 0,
  "secondaryTimeSource": 1,

  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "hostname": "sqm-esp32",
    "autoReconnect": true,
    "reconnectDelayMs": 1000,
    "maxReconnectDelayMs": 300000
  },

  "ntp": {
    "enabled": true,
    "server1": "pool.ntp.org",
    "server2": "time.nist.gov",
    "timezone": "UTC0",
    "gmtOffsetSec": 0,
    "daylightOffsetSec": 0,
    "syncIntervalMs": 600000
  },

  "gps": {
    "enabled": false,
    "rxPin": 17,
    "txPin": 16,
    "baudRate": 9600
  },

  "mqtt": {
    "enabled": false,
    "broker": "mqtt.example.com",
    "port": 1883,
    "username": "",
    "password": "",
    "topic": "sqm/data",
    "publishIntervalMs": 60000
  },

  "ota": {
    "enabled": false,
    "password": ""
  },

  "auth": {
    "enabled": false,
    "username": "admin",
    "password": ""
  },

  "rain": {
    "enabled": false,
    "rxPin": 18,
    "txPin": 19,
    "baudRate": 9600,
    "mode": "polling",
    "resolution": "high",
    "units": "metric"
  },

  "sensor": {
    "readIntervalMs": 5000,
    "i2cSDA": 21,
    "i2cSCL": 22,
    "i2cFrequency": 100000
  }
}
```

---

## Fields

### Device

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | string | `"SQM-ESP32"` | Friendly name shown in the web UI |
| `timezone` | string | `"UTC"` | Display timezone |
| `primaryTimeSource` | int | `0` | Primary time source: `0` = NTP, `1` = GPS |
| `secondaryTimeSource` | int | `1` | Fallback time source: `0` = NTP, `1` = GPS |

### WiFi

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssid` | string | — | Network name (2.4 GHz only) |
| `password` | string | — | Network password |
| `hostname` | string | `"sqm-esp32"` | mDNS hostname (`hostname.local`) |
| `autoReconnect` | bool | `true` | Reconnect on WiFi drop |
| `reconnectDelayMs` | int | `1000` | Initial reconnect delay (ms) |
| `maxReconnectDelayMs` | int | `300000` | Max reconnect backoff — 5 min |

### NTP

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `true` | Enable NTP time sync |
| `server1` | string | `"pool.ntp.org"` | Primary NTP server |
| `server2` | string | `"time.nist.gov"` | Fallback NTP server |
| `timezone` | string | `"UTC0"` | POSIX timezone string |
| `gmtOffsetSec` | int | `0` | UTC offset in seconds (e.g. `-28800` for PST) |
| `daylightOffsetSec` | int | `0` | DST offset in seconds |
| `syncIntervalMs` | int | `600000` | Re-sync interval — default 10 minutes |

!!! tip "POSIX timezone strings"
    Examples: `GMT0`, `EST5EDT,M3.2.0,M11.1.0`, `PST8PDT,M3.2.0,M11.1.0`, `CET-1CEST,M3.5.0,M10.5.0/3`.
    A full list is available at [timezonedb.com](https://timezonedb.com).

### GPS

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable GPS module |
| `rxPin` | int | `17` | ESP32 RX pin (connects to GPS TX) |
| `txPin` | int | `16` | ESP32 TX pin (connects to GPS RX) |
| `baudRate` | int | `9600` | GPS module baud rate (typically 9600) |

When GPS is enabled and has a fix, it can serve as the primary time source for accurate timestamps independent of network connectivity.

### MQTT

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable MQTT publishing |
| `broker` | string | — | Broker hostname or IP |
| `port` | int | `1883` | Broker port |
| `username` | string | `""` | Auth username (leave empty if none) |
| `password` | string | `""` | Auth password |
| `topic` | string | `"sqm/data"` | Publish topic |
| `publishIntervalMs` | int | `60000` | Publish interval — default 1 min |

### ArduinoOTA

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable command-line ArduinoOTA uploads |
| `password` | string | `""` | Required when command-line ArduinoOTA is enabled |

ArduinoOTA is disabled unless both `ota.enabled` is `true` and `ota.password` is set. The web UI OTA upload page is separate from command-line ArduinoOTA.

### HTTP Authentication

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Require credentials for mutation endpoints |
| `username` | string | `"admin"` | Username for HTTP Basic Auth |
| `password` | string | `""` | Password (required when auth is enabled) |

When `auth.enabled` is `true`, the following endpoints require HTTP Basic Auth credentials:

- `POST /api/config` — save configuration
- `POST /api/restart` — restart device
- `POST /api/update` — firmware OTA upload
- `POST /api/update/fs` — filesystem OTA upload
- `POST /api/wifi/connect` — change WiFi network
- `POST /api/mqtt/test` — test MQTT connection

Read-only endpoints remain accessible without credentials:

- `GET /api/sensors`, `GET /api/status`, `GET /api/config`
- WebSocket streams (`/ws/sensors`, `/ws/status`)

The `auth.password` field is masked in `GET /api/config` responses. Send the mask placeholder or an empty string to preserve the stored password; send `null` to clear it.

### Rain Sensor

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable Hydreon RG-15 rain sensor |
| `rxPin` | int | `18` | ESP32 RX pin (connects to RG-15 TX) |
| `txPin` | int | `19` | ESP32 TX pin (connects to RG-15 RX) |
| `baudRate` | int | `9600` | RG-15 serial baud rate |
| `mode` | string | `"polling"` | `"polling"` or `"continuous"` |
| `resolution` | string | `"high"` | `"high"`, `"low"`, or `"switch"` |
| `units` | string | `"metric"` | `"metric"`, `"imperial"`, or `"switch"` |

### Sensor

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `readIntervalMs` | int | `5000` | How often sensors are polled (ms) |
| `i2cSDA` | int | `21` | SDA GPIO pin |
| `i2cSCL` | int | `22` | SCL GPIO pin |
| `i2cFrequency` | int | `100000` | I2C clock speed (Hz) |

---

## Security Notes

SQMeter is designed for a trusted LAN environment. See the [Security Guide](security.md) for a full threat model and setup recommendations.

- By default there is no HTTP authentication; read-only and mutation endpoints are both accessible to any LAN device.
- Enable `auth.enabled` with a strong password to protect mutation endpoints (config save, OTA, restart, WiFi change).
- `GET /api/config` redacts stored secrets (WiFi, MQTT, OTA, and auth passwords), but still exposes non-secret settings.
- HTTP traffic is plaintext, so credentials sent via config updates are visible to LAN observers. TLS is not supported on the ESP32 in this firmware.
- Command-line ArduinoOTA is disabled unless `ota.enabled` is `true` and `ota.password` is set.

Keep the device on a private network or isolated observatory VLAN. Do not expose port 80 or ArduinoOTA to the public internet.

Some settings are applied immediately, but hardware bus settings such as I2C pins, GPS pins, and RG-15 pins may require a restart because the sensors are initialised during firmware startup.

---

## Updating via API

```bash
# Read current config
curl http://sqmeter.local/api/config

# Partial update — only the fields you include are changed
curl -X POST http://sqmeter.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"deviceName": "backyard-sqm", "ntp": {"server1": "time.google.com"}}'
```

Password fields returned by `GET /api/config` are masked as `********`. Sending a masked or empty password back to `POST /api/config` preserves the stored value; send `null` for a password field only when you intentionally want to clear it.

---

## Backup & Restore

```bash
# Backup
curl http://sqmeter.local/api/config > config-backup.json

# Restore
curl -X POST http://sqmeter.local/api/config \
  -H "Content-Type: application/json" \
  -d @config-backup.json
```
