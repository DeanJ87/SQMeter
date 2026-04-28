# Configuration Reference

Configuration is stored in NVS under namespace `sqm`, key `config`. You can read and write it via the web UI Settings page or directly via the REST API.

```bash
# Read current config
curl http://device-ip/api/config

# Write updated config
curl -X POST http://device-ip/api/config \
  -H "Content-Type: application/json" \
  -d @config.json
```

## Example config.json

```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "password": "secret",
    "hostname": "sqmeter"
  },
  "mqtt": {
    "enabled": false,
    "broker": "192.168.1.10",
    "port": 1883,
    "topic": "sqm/sensors",
    "username": "",
    "password": ""
  },
  "ntp": {
    "enabled": true,
    "server": "pool.ntp.org",
    "timezone": "UTC"
  },
  "gps": {
    "enabled": false,
    "rxPin": 16,
    "txPin": 17,
    "baudRate": 9600
  },
  "sensor": {
    "sdaPin": 21,
    "sclPin": 22,
    "readInterval": 5000
  },
  "cloudDetection": {
    "enabled": true,
    "clearThreshold": 10.0,
    "overcastThreshold": 3.0
  }
}
```

## WiFi Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssid` | string | `""` | WiFi network name |
| `password` | string | `""` | WiFi password |
| `hostname` | string | `"sqmeter"` | mDNS hostname (reachable at `hostname.local`) |

## MQTT Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable MQTT publishing |
| `broker` | string | `""` | Broker IP address or hostname |
| `port` | int | `1883` | Broker TCP port |
| `topic` | string | `"sqm/sensors"` | Base publish topic |
| `username` | string | `""` | MQTT username (leave empty for anonymous) |
| `password` | string | `""` | MQTT password |

## NTP Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `true` | Enable NTP time sync |
| `server` | string | `"pool.ntp.org"` | NTP server hostname |
| `timezone` | string | `"UTC"` | POSIX timezone string (e.g., `"EST5EDT"`) |

## GPS Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable GPS module |
| `rxPin` | int | `16` | ESP32 GPIO pin connected to GPS TX |
| `txPin` | int | `17` | ESP32 GPIO pin connected to GPS RX |
| `baudRate` | int | `9600` | GPS module baud rate |

When GPS is enabled and a fix is acquired, GPS time is used instead of NTP.

## Sensor Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sdaPin` | int | `21` | I2C SDA GPIO pin |
| `sclPin` | int | `22` | I2C SCL GPIO pin |
| `readInterval` | int | `5000` | Sensor polling interval in milliseconds |

## Cloud Detection Settings

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `true` | Enable MLX90614 cloud detection |
| `clearThreshold` | float | `10.0` | Temp differential (°C) above which sky is clear |
| `overcastThreshold` | float | `3.0` | Temp differential (°C) below which sky is overcast |

Cloud condition is derived from the difference between ambient temperature (BME280) and sky-pointing IR temperature (MLX90614):

- `differential > clearThreshold` → **Clear**
- `differential < overcastThreshold` → **Overcast**
- Between thresholds → **Cloudy**
