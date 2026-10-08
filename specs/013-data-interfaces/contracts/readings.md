# Contract: Readings document

Served identically by `GET /api/sensors`, `/ws/sensors` (every 1 s) and MQTT `<base>/state`.
Built by one serializer (`lib/Readings`). camelCase keys; each value under exactly one name.

```json
{
  "timestamp": 1791401772,
  "timeValid": true,
  "dataAgeMs": 412,
  "dataStale": false,
  "light":       { "status": "ok", "ageMs": 400, "lux": 0.0003, "visible": 307, "infrared": 47, "full": 357,
                   "gain": "MAX", "gainFactor": 9876, "integrationMs": 600, "saturated": false, "nightMode": true },
  "sky":         { "status": "ok", "sqm": 21.48, "rawSqm": 21.41, "nelm": 6.2, "bortle": 2,
                   "description": "Typical truly dark site", "calibrated": false, "averagingWindowSeconds": 90 },
  "environment": { "status": "ok", "ageMs": 3100, "temperature": 12.3, "humidity": 64.7, "pressure": 1013.4,
                   "dewpoint": 6.1 },
  "infrared":    { "status": "ok", "ageMs": 3100, "skyTemperature": -24.7, "ambientTemperature": 12.4 },
  "clouds":      { "status": "ok", "coverPercent": 3, "condition": "clear", "description": "Clear",
                   "temperatureDelta": -37.1, "correctedDelta": -33.0, "humidity": 64.7, "humiditySource": "measured" },
  "gps":         { "status": "ok", "ageMs": 750, "fix": true, "satellites": 9, "latitude": 51.5074,
                   "longitude": -0.1278, "altitude": 42.0, "hdop": 1.1 },
  "rain":        { "status": "ok", "ageMs": 40, "raining": false, "rainingNow": false, "intensity": 0.0,
                   "eventAccumulation": 0.4, "sensorEventAccumulation": 0.4, "totalAccumulation": 12.6,
                   "lensFault": false, "emitterSaturated": false },
  "wind":        { "status": "ok", "ageMs": 900, "speed": 3.3, "gust": 6.8, "direction": 246, "vaneFault": false },
  "safety":      { "...": "REST and /ws/sensors only - see GET /api/safety" }
}
```

## Rules

- `timestamp`: Unix seconds, always. `timeValid` is false (and `timestamp` 0) until the clock is set.
- `status`: `ok` | `missing` (not detected / not responding) | `error` (read error, invalid data) | `stale`.
  When `status` isn't `ok`, the group carries only `status` and, unless it's `missing` (never answered), `ageMs` — never zeros.
- Presence: `light`, `sky`, `environment`, `infrared`, `clouds` always (built-in sensors). `gps` only
  when GPS is enabled, `rain` only when the rain sensor is enabled, `wind` only when the anemometer is.
  `sky` follows `light`'s status; `clouds` follows `infrared`'s.
- Units: °C, %, hPa, lux, mag/arcsec², m/s, degrees (0 = north, clockwise), mm and mm/h (the
  RG-15's imperial output is converted). `wind.direction` is omitted when calm or no vane.
- `clouds.condition`: `clear` | `cloudy` | `overcast`, from the configured thresholds.
  `clouds.humiditySource`: `measured` | `assumed` (53% without the environment sensor).
- `rain.raining` is the latched state (held for the clear delay); `rainingNow` is the instantaneous
  one. `eventAccumulation` is the device's event total (cleared by the clear delay);
  `sensorEventAccumulation` is the RG-15's own.
- MQTT `<base>/state` contains the groups enabled in the MQTT settings and no `safety` group
  (safety has its own topics).
- Diagnostics (rolling light counts, RG-15 UART counters and raw responses) are not part of this
  document: see `/api/status` → `diagnostics` and MQTT `<base>/diagnostics`.
