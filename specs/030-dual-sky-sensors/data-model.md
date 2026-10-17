# Data model: Two sky sensors (SQM and SQM-L)

Decisions are in [research.md](research.md) (D-numbers below). Externally visible shapes are in
[contracts/](contracts/).

## SkyRole (lib/SensorTypes)

`enum class SkyRole : uint8_t { Sqm = 0, Sqml = 1 }`, `SKY_ROLE_COUNT = 2`.
Names: `"sqm"` / `"sqml"` (JSON, config, `sky.source`); labels "SQM" / "SQM-L" (UI, HA, Alpaca).
`skyRoleName(SkyRole)`, `parseSkyRole(const char *, SkyRole &)` (false on anything else).

## Sky sensor (firmware driver, src/sensors/TSL2591Sensor)

| Field | Type | Notes |
|---|---|---|
| role | `SkyRole` | fixed at construction |
| bus | `TwoWire &` | `Wire` for Sqm, `Wire1` for Sqml (D1) |
| enabled | bool | from `cfg.sky.sqmEnabled` / `sqmlEnabled`; disabled = never begun or read |
| samples | `unique_ptr<SampleBuffers>` | 3 x 512 x u16, allocated in `begin()` (D16) |
| cycle | `SkyReadCycle` | pure timing + retry state (below) |
| reading | `TSL2591Reading` | unchanged struct |
| averaging, calibration | `SkyAveragingConfig`, `SkyCalibrationConfig` | per role (FR-003) |

### SkyReadCycle (lib/SkyLogic, pure)

States: `Idle -> Integrating(startedMs) -> (collect) -> Idle`; `Absent(retryAtMs, backoffMs)`.

| Input | Transition |
|---|---|
| `due(now)` in Idle, cadence elapsed | start: enable ADC, -> Integrating |
| `ready(now)` in Integrating, `now - startedMs >= collectDelayMs(integrationMs)` (integration + 20% clock tolerance + `MARGIN_MS`, 740 ms at 600 ms) | collect |
| collect ok | -> Idle, `consecutiveFailures = 0` |
| collect fails | status ReadError; `++consecutiveFailures`; at `REBEGIN_AFTER` (3) -> Absent(backoff 5 s) |
| Absent, `now >= retryAtMs` | try `begin()`: ok -> Idle; fail -> backoff x2, cap 60 s |

Not-yet-begun sensors start in Absent with `retryAtMs = 0` (try at once).

## SkySensorSnapshot (lib/DeviceCore)

Per role, in `SensorSnapshot::skySensors[SKY_ROLE_COUNT]`:

| Field | Type |
|---|---|
| enabled | bool |
| initialized | bool (begun and answering) |
| reading | `TSL2591Reading` |
| diagnostics | `TSL2591Diagnostics` |
| lastUpdate | uint32_t (millis of last good read, 0 = never) |
| metrics | `SkyQualityMetrics` (derived) |

The existing `SensorSnapshot::tsl`, `tslDiagnostics`, `tslInitialized`, `tslLastUpdate`, `sky`
become the **authoritative copy** (D3), set by `Core::derive`:

- `cfg.sky.authoritative == role` and that role enabled -> copy of that role's fields.
- authoritative role disabled -> `tsl = TSL2591Reading{}` (status NotInitialized, timestamp 0),
  `tslInitialized = false`, `tslLastUpdate = 0`, `sky = SkyQualityMetrics{}` (FR-010).
- `SensorSnapshot::skySource` = authoritative role (always set, even when it has no reading).

## Config (lib/ConfigModel)

| Key | Type | Default | Validation |
|---|---|---|---|
| `sensor.i2c2SDA` | u8 | 255 (unset) | 255 or valid GPIO; pair rule (D5) |
| `sensor.i2c2SCL` | u8 | 255 (unset) | as above; not equal to SDA or any other used pin |
| `sky.sqmEnabled` | bool | true | |
| `sky.sqmlEnabled` | bool | false | true requires both I2C-2 pins set |
| `sky.authoritative` | `SkyRole` (`"sqm"`/`"sqml"`) | `"sqm"` | unknown string rejected |
| `sky.showOther` | bool | true | |
| `skyAveragingL.windowSeconds` | u16 | 90 | 10..300 |
| `skyCalibrationL.enabled` | bool | false | |
| `skyCalibrationL.sqmOffset` | float | 0 | -5..5 |
| `skyCalibrationL.darkVisibleOffset`, `darkFullOffset`, `darkIrOffset` | float | 0 | >= 0, finite |
| `skyCalibrationL.darkSampleCount` | u32 | 0 | |
| `skyCalibrationL.darkCalibratedAt` | i64 | 0 | |

`Config` gains `SkyConfig sky; SkyAveragingConfig skyAveragingL; SkyCalibrationConfig skyCalibrationL;`
and `sensor.i2c2SDA/i2c2SCL`. Helpers: `averaging(SkyRole)`, `calibration(SkyRole)` return the
role's section; `skyEnabled(SkyRole)`. Pin conflict checks (I2C-1, I2C-2, GPS, rain, wind) treat
255 as unused. Missing keys keep defaults (FR-014).

## Readings::Snapshot (lib/Readings)

Added:

- `SkySource source` (`Sqm`/`Sqml`) -> `sky.source`.
- `SkySensorEntry skySensors[2]`:
  `present` (enabled), `status`, `ageMs`, `sqm`, `rawSqm`, `nelm`, `bortle`, `description`,
  `calibrated`, `averagingWindowSeconds`, `lux`, `gain`, `integrationMs`, `saturated`, `nightMode`.
- `bool deltaValid; double deltaSqm` (SQM-L − SQM, both entries ok).
- `Groups` gains `skySqm`, `skySqml` (HA entity presence, D7).
- `constexpr size_t DOC_CAPACITY` (sized by test, D6).

`light` and `sky` keep their fields and are filled from the authoritative copy (unchanged code
path).

## Safety / Alpaca / alerts inputs (lib/AlpacaLogic, lib/AlertLogic)

- `Alpaca::SafetyInputs`: + `SkyRole skyRole`, `bool skyNamed` (both sensors enabled). Used only
  to pick the literal reason (D4).
- `Alpaca::ObservingConditionsSnapshot`: + `SkyRole skyRole`, `bool skyNamed` (D8).
- `Alerts::SENSOR_COUNT` = 6; slot 5 = SQM-L (D10). `Core::alertVars` adds `sqm_wide`, `sqm_l`,
  `sqm_delta`, `sqm_source`.

## Settings dependency facts (lib/SettingsDeps)

`Deps::Facts` + `i2c2Configured`, `sqmEnabled`, `sqmlEnabled`, `sqmDetected`, `sqmlDetected`;
`lightDetected` = authoritative sensor detected (D13).

## Web types (web/src/types)

- `SkyReading.source?: 'sqm' | 'sqml'`; `SensorData.skySensors?: { sqm?: SkySensorReading;
  sqml?: SkySensorReading; deltaSqm?: number }`.
- `Status.skySensors?: { sqm?: SkySensorHealth; sqml?: SkySensorHealth }`.
- `Config.sensor.i2c2SDA/i2c2SCL`, `Config.sky`, `Config.skyAveragingL`, `Config.skyCalibrationL`
  (Zod: `web/src/validation/configSchema.ts`, defaults: `web/src/components/settings/defaults.ts`).

## State transitions: authoritative choice

```text
             owner selects SQM-L (both enabled)
  [SQM] -------------------------------------------> [SQM-L]
    ^                                                   |
    +---------------------------------------------------+
             owner selects SQM / disables SQM-L

  disabling the authoritative sensor does NOT switch the choice:
  single-value fields report no reading (FR-010) until the owner picks.
```

Disabling SQM-L while it is authoritative: Settings saves `sky.authoritative` unchanged but the
dependency D-40 shows the choice disabled with its reason; the dashboard offers "Use SQM".
