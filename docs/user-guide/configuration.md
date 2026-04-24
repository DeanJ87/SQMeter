# Configuration

All settings are stored in NVS (Non-Volatile Storage) and survive firmware and filesystem updates. Configure via the web UI Settings page or the REST API.

---

## Full Configuration Reference

```json
{
  "deviceName": "SQMeter",
  "timezone": "UTC",
  "primaryTimeSource": 0,
  "secondaryTimeSource": 1,

  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "hostname": "sqmeter",
    "autoReconnect": true,
    "reconnectDelayMs": 1000,
    "maxReconnectDelayMs": 300000
  },

  "ntp": {
    "enabled": true,
    "server1": "pool.ntp.org",
    "server2": "time.cloudflare.com",
    "timezone": "GMT0",
    "gmtOffsetSec": 0,
    "daylightOffsetSec": 3600,
    "syncIntervalMs": 3600000
  },

  "gps": {
    "enabled": false,
    "rxPin": 16,
    "txPin": 17,
    "baudRate": 9600
  },

  "mqtt": {
    "enabled": false,
    "broker": "mqtt.example.com",
    "port": 1883,
    "username": "",
    "password": "",
    "topic": "sqmeter/data",
    "publishIntervalMs": 60000
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
| `deviceName` | string | `"SQMeter"` | Friendly name shown in the web UI |
| `timezone` | string | `"UTC"` | Display timezone |
| `primaryTimeSource` | int | `0` | Primary time source: `0` = NTP, `1` = GPS |
| `secondaryTimeSource` | int | `1` | Fallback time source: `0` = NTP, `1` = GPS |

### WiFi

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssid` | string | — | Network name (2.4 GHz only) |
| `password` | string | — | Network password |
| `hostname` | string | `"sqmeter"` | mDNS hostname (`hostname.local`) |
| `autoReconnect` | bool | `true` | Reconnect on WiFi drop |
| `reconnectDelayMs` | int | `1000` | Initial reconnect delay (ms) |
| `maxReconnectDelayMs` | int | `300000` | Max reconnect backoff — 5 min |

### NTP

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `true` | Enable NTP time sync |
| `server1` | string | `"pool.ntp.org"` | Primary NTP server |
| `server2` | string | `"time.cloudflare.com"` | Fallback NTP server |
| `timezone` | string | `"GMT0"` | POSIX timezone string |
| `gmtOffsetSec` | int | `0` | UTC offset in seconds (e.g. `-28800` for PST) |
| `daylightOffsetSec` | int | `3600` | DST offset in seconds |
| `syncIntervalMs` | int | `3600000` | Re-sync interval — default 1 hour |

!!! tip "POSIX timezone strings"
    Examples: `GMT0`, `EST5EDT,M3.2.0,M11.1.0`, `PST8PDT,M3.2.0,M11.1.0`, `CET-1CEST,M3.5.0,M10.5.0/3`.
    A full list is available at [timezonedb.com](https://timezonedb.com).

### GPS

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable GPS module |
| `rxPin` | int | `16` | ESP32 RX pin (connects to GPS TX) |
| `txPin` | int | `17` | ESP32 TX pin (connects to GPS RX) |
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
| `topic` | string | `"sqmeter/data"` | Publish topic |
| `publishIntervalMs` | int | `60000` | Publish interval — default 1 min |

### Sensor

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `readIntervalMs` | int | `5000` | How often sensors are polled (ms) |
| `i2cSDA` | int | `21` | SDA GPIO pin |
| `i2cSCL` | int | `22` | SCL GPIO pin |
| `i2cFrequency` | int | `100000` | I2C clock speed (Hz) |

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
