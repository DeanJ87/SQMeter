# Configuration

All settings are stored in NVS (Non-Volatile Storage) and survive firmware and filesystem updates. Configure via the web UI Settings page or the REST API.

---

## Full Configuration Reference

```json
{
  "deviceName": "SQM-ESP32",
  "timezone": "UTC",

  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "hostname": "sqm-esp32",
    "autoReconnect": true,
    "reconnectDelayMs": 1000,
    "maxReconnectDelayMs": 300000
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
| `timezone` | string | `"UTC"` | Timezone string (POSIX format) |

### WiFi

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssid` | string | — | Network name (2.4 GHz only) |
| `password` | string | — | Network password |
| `hostname` | string | `"sqm-esp32"` | mDNS hostname (`hostname.local`) |
| `autoReconnect` | bool | `true` | Reconnect on WiFi drop |
| `reconnectDelayMs` | int | `1000` | Initial reconnect delay |
| `maxReconnectDelayMs` | int | `300000` | Max reconnect backoff (5 min) |

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
curl http://sqm-esp32.local/api/config

# Write updated config
curl -X POST http://sqm-esp32.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"deviceName": "backyard-sqm"}'
```

You can POST partial updates — only the fields you include are changed.

---

## Backup & Restore

```bash
# Backup
curl http://sqm-esp32.local/api/config > config-backup.json

# Restore
curl -X POST http://sqm-esp32.local/api/config \
  -H "Content-Type: application/json" \
  -d @config-backup.json
```
