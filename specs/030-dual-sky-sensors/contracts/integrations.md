# Contract: Home Assistant, Alpaca, SafetyMonitor, BLE, alerts

## Home Assistant discovery (lib/Readings `ENTITIES[]`)

Unchanged ids and templates: `sqm` ("Sky quality", `value_json.sky.sqm`), `nelm`, `bortle`,
`illuminance` (authoritative sensor).

Added (component `sensor`, state topic `<base>/state`):

| object id | Name | value | unit | present when |
|---|---|---|---|---|
| `sqm_wide` | SQM (wide) | `value_json.skySensors.sqm.sqm` | mag/arcsec² | sky group on and SQM enabled |
| `sqm_l` | SQM-L (narrow) | `value_json.skySensors.sqml.sqm` | mag/arcsec² | sky group on and SQM-L enabled |
| `sqm_delta` | SQM-L − SQM | `value_json.skySensors.deltaSqm` | mag/arcsec² | both of the above |

Availability: `{{ 'online' if value_json.skySensors.<role>.status == 'ok' else 'offline' }}`
(`sqm_delta`: both). Not present -> empty retained config (entity removed). Discovery is
re-published when the config changes (existing `publishDiscovery` path), so enabling or disabling
a sensor adds or removes its entities without a restart.

## Alpaca ObservingConditions

- `skyquality`, `skybrightness`: authoritative sensor; NotImplemented when it is disabled (not
  present), driver error 0x500-range "no data" when faulted/stale (same as a TSL2591 today).
- `SensorDescription`:

| Situation | skybrightness | skyquality |
|---|---|---|
| SQM authoritative, SQM-L disabled | `TSL2591 light sensor` | `TSL2591 light sensor, calibrated SQM` |
| SQM authoritative, both enabled | `TSL2591 SQM (wide)` | `TSL2591 SQM (wide), calibrated SQM` |
| SQM-L authoritative | `TSL2591 SQM-L (narrow)` | `TSL2591 SQM-L (narrow), calibrated SQM` |

- `TimeSinceLastUpdate` follows the authoritative sensor. Interface version unchanged; ConformU zero
  errors/issues.

## SafetyMonitor

- Required sensors: authoritative sky sensor and the IR sky sensor (as today, with "light sensor"
  meaning the authoritative one). A non-authoritative sensor never affects the verdict.
- Reasons (literal templates, research D4):
  - `Sensor fault: light sensor` / `... light sensor and IR sky sensor` (one sensor, unchanged)
  - `Sensor fault: SQM sensor` / `Sensor fault: SQM sensor and IR sky sensor`
  - `Sensor fault: SQM-L sensor` / `Sensor fault: SQM-L sensor and IR sky sensor`
  - `SQM %.2f < %.2f` (SQM authoritative, unchanged) / `SQM-L %.2f < %.2f`
- Freshness (`requiredDataAge`) uses the authoritative sensor.

## BLE

- Advertisement: unchanged bytes; `sqm` = authoritative (0xFFFF when it has no reading).
- Summary characteristic JSON: `sqm` (authoritative) plus `"sqmSource": "sqm"|"sqml"` whenever
  `sqm` is present.

## Alerts

Template variables (common to every event):

| Variable | Value |
|---|---|
| `{sqm}` | authoritative SQM (unchanged) |
| `{sqm_min}` | minimum-SQM limit (unchanged) |
| `{sqm_wide}` | SQM sensor's SQM, 2 decimals |
| `{sqm_l}` | SQM-L sensor's SQM, 2 decimals |
| `{sqm_delta}` | SQM-L − SQM, signed, 2 decimals |
| `{sqm_source}` | `SQM` or `SQM-L` |

A value without a reading renders as `{sqm}` does today without one. Sensor events: slot names
`Light sensor` (SQM, SQM-L disabled) or `SQM sensor`, and `SQM-L sensor` (enabled only) for
`{sensor}` in `sensor_fault` / `sensor_recovered`.
