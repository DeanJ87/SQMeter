# Contract: readings document (`GET /api/sensors`, `/ws/sensors`, MQTT `<base>/state`)

One serializer (`lib/Readings::write`), so the three interfaces stay identical (spec 013).

## Single-value fields (FR-007)

`light` and `sky` keep every field, name, type, unit and presence rule. They carry the
**authoritative** sensor. Added: `sky.source` (`"sqm"` | `"sqml"`), always written with the `sky`
group, also when the sensor is not ok.

```json
"sky": { "status": "ok", "source": "sqml", "sqm": 21.48, "rawSqm": 21.48, "nelm": 6.6,
         "bortle": 3, "description": "Rural sky", "calibrated": false, "averagingWindowSeconds": 90 }
```

Authoritative sensor disabled, faulted or stale (FR-010): exactly today's not-ok form, never the
other sensor's values:

```json
"light": { "status": "missing" },
"sky":   { "status": "missing", "source": "sqml" }
```

## Per-sensor readings (FR-008)

Written with the `sky` group. An entry exists only for an **enabled** sensor; `skySensors` is
omitted when neither is enabled. Each entry follows the "status (and ageMs) only when not ok"
rule. `deltaSqm` (SQM-L − SQM, 2 decimals) only when both entries are `ok`.

```json
"skySensors": {
  "sqm":  { "status": "ok", "ageMs": 412, "sqm": 21.23, "rawSqm": 21.23, "nelm": 6.5, "bortle": 3,
            "description": "Rural sky", "calibrated": false, "averagingWindowSeconds": 90,
            "lux": 0.00041, "gain": "MAX", "integrationMs": 600, "saturated": false, "nightMode": true },
  "sqml": { "status": "ok", "ageMs": 398, "sqm": 21.48, "...": "same fields" },
  "deltaSqm": 0.25
}
```

```json
"skySensors": { "sqm": { "status": "ok", "...": "..." }, "sqml": { "status": "error", "ageMs": 5120 } }
```

Rounding matches `sky`/`light`: `sqm`, `rawSqm` 2; `nelm` 1; `lux` 6.

## MQTT groups

`mqtt.publish.sky` controls `light`, `sky` and `skySensors` together. No new publish setting.

## Schemas

`specs/016-demo-device-emulation/contracts/schemas/readings.schema.json`: `sky.source` (optional,
enum) and `skySensors` (optional object; `sqm`, `sqml` optional entries with `status` required;
`deltaSqm` optional number). No existing property changes.

## Size

The worst-case document (all groups ok, both sky sensors) fits `Readings::DOC_CAPACITY` with 20%
headroom and its MQTT payload fits the 3072-byte client buffer (native test, research D6).
