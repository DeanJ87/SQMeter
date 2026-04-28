# API Reference

## REST Endpoints

All endpoints are served on port 80. Requests and responses use JSON unless noted.

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/status` | System status (uptime, free heap, WiFi RSSI) |
| `GET` | `/api/sensors` | Current sensor readings snapshot |
| `GET` | `/api/config` | Read full device configuration |
| `POST` | `/api/config` | Write device configuration |
| `POST` | `/api/restart` | Restart the device |
| `GET` | `/api/wifi/scan` | Scan for nearby WiFi networks |
| `POST` | `/api/wifi/connect` | Connect to a WiFi network |
| `POST` | `/api/update` | OTA firmware update (multipart/form-data) |

### Example: Read Configuration

```bash
curl http://device-ip/api/config
```

### Example: Update Configuration

```bash
curl -X POST http://device-ip/api/config \
  -H "Content-Type: application/json" \
  -d '{"mqtt":{"enabled":true,"broker":"192.168.1.10"}}'
```

### Example: Trigger Restart

```bash
curl -X POST http://device-ip/api/restart
```

## WebSocket

Connect to `/ws/sensors` for real-time sensor updates pushed at a 1-second interval.

```javascript
const ws = new WebSocket('ws://device-ip/ws/sensors');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Sky quality:', data.skyQuality.sqm);
};
```

### Payload Format

```json
{
  "lightSensor": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "timestamp": 12345,
    "status": 0
  },
  "environment": {
    "temperature": 22.5,
    "humidity": 45.2,
    "pressure": 1013.25,
    "timestamp": 12345,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2,
    "description": "Typical truly dark site"
  },
  "irTemperature": {
    "ambientTemp": 18.3,
    "objectTemp": 5.1,
    "timestamp": 12345,
    "status": 0
  },
  "cloudConditions": {
    "differential": 13.2,
    "condition": "Clear",
    "confidence": 0.92
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `lightSensor.lux` | float | Calibrated lux reading from TSL2591 |
| `lightSensor.visible` | int | Raw visible channel counts |
| `lightSensor.infrared` | int | Raw IR channel counts |
| `environment.temperature` | float | Ambient temperature in °C |
| `environment.humidity` | float | Relative humidity % |
| `environment.pressure` | float | Atmospheric pressure in hPa |
| `skyQuality.sqm` | float | Sky Quality Meter value (mag/arcsec²) |
| `skyQuality.nelm` | float | Naked Eye Limiting Magnitude |
| `skyQuality.bortle` | int | Bortle dark sky class (1–9) |
| `irTemperature.ambientTemp` | float | MLX90614 ambient temperature °C |
| `irTemperature.objectTemp` | float | MLX90614 sky-pointing object temperature °C |
| `cloudConditions.differential` | float | Temp differential used for cloud detection |
| `cloudConditions.condition` | string | `"Clear"`, `"Cloudy"`, or `"Overcast"` |
| `cloudConditions.confidence` | float | Detection confidence 0.0–1.0 |

## MQTT

Enable MQTT in the device settings to publish sensor data to a broker.

**Topic:** Configurable via `mqtt.topic` in `config.json` (e.g., `sqm/sensors`)

**Publish interval:** Same as sensor read interval (default 5 seconds)

### MQTT Payload

```json
{
  "timestamp": 12345,
  "light": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45
  },
  "sky": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2
  },
  "environment": {
    "temperature": 22.5,
    "humidity": 45.2,
    "pressure": 1013.25
  }
}
```

MQTT publishes use QoS 0 (fire-and-forget) by default.
