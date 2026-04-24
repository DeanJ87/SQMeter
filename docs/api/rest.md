# REST API

All endpoints are on port 80. Base URL: `http://<device-ip>/api`

---

## Endpoints

### `GET /api/status`

System status — uptime, memory, WiFi signal.

```bash
curl http://sqm-esp32.local/api/status
```

```json
{
  "uptime": 3600,
  "freeHeap": 210432,
  "rssi": -62,
  "ip": "192.168.1.42",
  "version": "0.0.1"
}
```

---

### `GET /api/sensors`

Current sensor readings (point-in-time snapshot).

```bash
curl http://sqm-esp32.local/api/sensors
```

```json
{
  "tsl2591": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "timestamp": 12345,
    "status": 0
  },
  "bme280": {
    "temperature": 12.4,
    "humidity": 72.1,
    "pressure": 1013.25,
    "timestamp": 12345,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site"
  }
}
```

---

### `GET /api/config`

Read the full device configuration.

```bash
curl http://sqm-esp32.local/api/config
```

---

### `POST /api/config`

Update configuration. Partial updates are supported — only the fields you send are changed.

```bash
curl -X POST http://sqm-esp32.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"deviceName": "backyard-sqm", "sensor": {"readIntervalMs": 10000}}'
```

---

### `GET /api/wifi/scan`

Scan for nearby WiFi networks.

```bash
curl http://sqm-esp32.local/api/wifi/scan
```

```json
[
  { "ssid": "MyNetwork", "rssi": -55, "secure": true },
  { "ssid": "Neighbour", "rssi": -80, "secure": true }
]
```

---

### `POST /api/wifi/connect`

Connect to a WiFi network and save credentials to NVS.

```bash
curl -X POST http://sqm-esp32.local/api/wifi/connect \
  -H "Content-Type: application/json" \
  -d '{"ssid": "MyNetwork", "password": "hunter2"}'
```

---

### `POST /api/restart`

Restart the device.

```bash
curl -X POST http://sqm-esp32.local/api/restart
```

---

### `POST /api/update`

OTA firmware update. Send a raw `.bin` file as `multipart/form-data`.

```bash
curl -X POST http://sqm-esp32.local/api/update \
  -F "firmware=@sqmv2-firmware-v0.0.1.bin"
```

The device reboots automatically on success.
