# Contract: `/api/status` and the dark-calibration route

## `GET /api/status`

Unchanged: `sensors.light` and `diagnostics.light` (now the authoritative sensor, research D12).

Added: `skySensors`, one entry per **enabled** sensor:

```json
"skySensors": {
  "sqm":  { "status": "ok", "ageMs": 412, "detected": true, "gain": "MAX", "integrationMs": 600,
            "saturated": false, "nightMode": true, "sampleCount": 150, "windowSamples": 150,
            "darkVisibleOffset": 0.0 },
  "sqml": { "status": "missing", "detected": false }
}
```

- `status` uses the readings statuses (`ok`, `missing`, `error`, `stale`); `ageMs` omitted while
  `missing` (as `sensors.*`).
- `detected`: begun and answering on its bus (Settings rows, settings-dependency facts).
- Diagnostics fields only when `detected`.

`status.schema.json` gains `skySensors` (optional). Dashboard inventory (DASH-01/02) maps the new
paths (shown: the Sky card and glance tiles; not shown: `sampleCount`, `windowSamples`,
`darkVisibleOffset`, which Settings shows).

## `POST /api/sensors/tsl2591/calibrate-dark[?sensor=sqm|sqml]`

Auth: `requireAuth` (unchanged).

| Request | Result |
|---|---|
| no `sensor` | calibrates the SQM, as today (FR-009) |
| `sensor=sqm` / `sensor=sqml` | calibrates that sensor; writes only its `skyCalibration` / `skyCalibrationL` (US3 AC2) |
| `sensor=` anything else | 400 `{"error":"Unknown sky sensor"}` |
| sensor disabled, not detected or no samples | 409 `{"error":"No light-sensor readings to calibrate from"}` |
| not in night mode / window not full | 409, existing messages |
| ok | 200, existing body plus `"sensor": "sqm"|"sqml"` |

OpenAPI: `tools/api/operations.json` documents the optional `sensor` query parameter (enum) and
the 400; `docs/api/openapi.json` regenerated and checked.
