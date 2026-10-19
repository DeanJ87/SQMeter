# Research: Two sky sensors (SQM and SQM-L)

Grounded in the code as of `feature/v0.4-alpha` (3aa9481). Paths are repo-relative.

## What exists today

- `src/sensors/TSL2591Sensor.cpp` owns one `Adafruit_TSL2591` on the global `Wire` (`begin()` with
  no bus argument), its 512-sample rolling window (3 x 1 KB arrays), auto-ranging, the SQM offset
  and the dark offset (`configureSkyMeasurement`). `src/main.cpp` creates one `tslSensor`, reads it
  every 650 ms (`TSL_SAMPLE_INTERVAL_MS`) and hands a reference to `WebServer::Sensors::tsl`.
- `Adafruit_TSL2591::getFullLuminosity()` **blocks** for the whole integration (`delay(120)` per
  step, ~720 ms at 600 ms integration) and **never reports a bus error**: `readSensor()` always
  returns `true`, so an unplugged sensor keeps status `Ok` and reads as darkness. The library
  only re-runs `begin()` when it was never initialised.
- `lib/DeviceCore` (`SensorSnapshot::tsl`, `tslDiagnostics`, `tslInitialized`, `tslLastUpdate`)
  turns the one reading into: `snapshot.sky` (`Core::derive` -> `SkyQuality::calculate(tsl.lux)`),
  readings `light`/`sky` (`readSkyGroups`), safety inputs (`skyLightFault`, `requiredSensorFault`,
  `sqm`, `requiredDataAge`), Alpaca `skyLight` source state + `skyQualityMagArcsec2` /
  `skyBrightnessLux`, alert inputs (`sensors[0] = "Light sensor"`, `{sqm}`), `/api/status`
  `sensors.light` and `diagnostics.light`, and settings-deps facts (`lightDetected`).
- `lib/Readings` writes `light` + `sky` (MQTT group `sky`) and the HA entities `sqm`, `nelm`,
  `bortle`, `illuminance` (`ENTITIES[]`, removed with an empty payload when the group is off).
- `lib/AlpacaLogic/src/ObservingConditionsMapper.cpp`: `skyquality` / `skybrightness` use the
  `skyLight` source with fixed descriptions ("TSL2591 light sensor, calibrated SQM").
- `lib/AlpacaLogic/src/SafetyEvaluator.cpp`: light sensor fault is a required-sensor fault
  ("Sensor fault: light sensor"); the minimum-SQM rule is skipped while it is faulted, so the
  verdict is already unsafe through the fault reason.
- BLE (`src/WebServerAlerts.cpp::updateBle`): advertisement `sqm` from `obs.skyQualityMagArcsec2`,
  summary JSON (`StaticJsonDocument<256>`) `sqm`.
- Dark calibration: `POST /api/sensors/tsl2591/calibrate-dark` (`WebServerApi.cpp`), no body,
  writes `skyCalibration.*`.
- Config (`lib/ConfigModel`): `sensor.i2cSDA/i2cSCL/i2cFrequency`, `skyAveraging.windowSeconds`,
  `skyCalibration.*`. Default main JSON is 2139 bytes, about 2850 bytes with long strings in every
  text field, against the 5100-byte limit (measured with a host build of `Config::toJson`).
- Readings document: worst case today (all groups ok) serialises to 1247 bytes and uses 2336 bytes
  of an ArduinoJson pool; MQTT `<base>/state` uses `DynamicJsonDocument(3072)`, REST/WS
  `createSensorDataJson` 4096 (with `safety`), the PubSubClient buffer is 3072.
- Web: `SkyHero` / `LightCard` / `SkyUnavailableCard` (`web/src/components/dashboard/skyCards.tsx`),
  `SkySensorsCard` / `SkyQualityCard` (`web/src/components/settings/SensorsSkyCards.tsx`),
  `useDarkCalibration` (`web/src/hooks/useSensorActions.ts`), glance sensors list
  (`web/src/dashboard/glance.ts`, `['light', 'infrared']`), inventory (`web/src/dashboard/inventory.json`),
  sensor names (`web/src/lib/sensorNames.ts`, glossary `tools/i18n/glossary/en.json`).
- Demo: `tools/demo-core/sensor_feed.cpp` fills `snapshot.tsl` from the simulator's `light`
  conditions; the WASM core is rebuilt by `tools/demo-core/build.sh` (emcc is installed here).

## D1 One driver class, two instances, explicit bus

**Decision**: `TSL2591Sensor` takes a `TwoWire &` and a role (`SkyRole::Sqm` / `SkyRole::Sqml`) in
its constructor; `begin()` calls `Adafruit_TSL2591::begin(&bus)`. The SQM uses `Wire`
(`sensor.i2cSDA/SCL`), the SQM-L uses `Wire1` (`sensor.i2c2SDA/SCL`). A disabled role is still
constructed (like GPS and the RG-15) but never begun or read, so `WebServer` always has an object.

**Rationale**: the TSL2591 address (0x29) is fixed; the ESP32 has two I2C controllers. One class
keeps auto-ranging, averaging and calibration identical (FR-001).

**Alternatives**: a TCA9548A multiplexer (extra hardware, not on Rev A); a second class (duplicate
logic).

## D2 Non-blocking, overlapped reads with fault detection

**Decision**: split a read into *start* (enable the ADC) and *collect* (after the integration time
plus margin, read CHAN0/CHAN1 with our own `TwoWire` transaction and check
`endTransmission() == 0` and `requestFrom() == 4`, then disable). The loop starts both enabled
sensors together and collects each when its integration is done, so two sensors take the same
~650 ms cycle as one and the loop never sleeps for the integration. A failed transaction is
`ReadError`; after `REBEGIN_AFTER` consecutive failures (or when never begun) the driver retries
`begin()` with a back-off (5 s, doubling to 60 s), so a replugged sensor recovers without a
restart (edge case). The timing and back-off decisions live in `lib/SkyLogic`
(`SkyReadCycle`: pure, native-tested); the driver only does I2C.

**Rationale**: two blocking reads would hold the loop ~1.45 s per cycle (web handle, MQTT,
wind sampling every second). Detecting bus errors is required for FR-010 and constitution I: today
an unplugged sensor reads as a dark sky, which is also a latent single-sensor safety bug that this
fixes for both roles.

**Alternatives**: a FreeRTOS task per sensor (more stack, shared-state locking); keep the library's
blocking read and alternate sensors each cycle (halves each sensor's sample rate and changes the
averaging window's meaning).

## D3 Where "authoritative" is chosen: DeviceCore, not the driver

**Decision**: `SensorSnapshot` carries `SkySensorSnapshot sky[2]` (reading, diagnostics,
enabled, initialized, lastUpdate) indexed by role, and keeps the existing `tsl`, `tslDiagnostics`,
`tslInitialized`, `tslLastUpdate` and `sky` fields as the **authoritative** copy, filled by
`Core::derive` from `cfg.sky.authoritative`. When the authoritative role is disabled, the copy is
an empty reading with `initialized = false` / status `NotInitialized` (FR-010: no fallback). All
existing consumers (readings `light`/`sky`, safety, Alpaca, BLE, alerts, status `sensors.light`,
`diagnostics.light`) keep reading the same fields and change source automatically (SC-003).

**Rationale**: one switch point, in `lib/*`, native-testable (constitution III); the firmware,
native tests and the WASM demo share it; minimal churn in the consumers.

**Alternatives**: resolve the source in every consumer (many places to get FR-010 wrong); swap
which driver object is "tsl" in `main.cpp` (not testable, breaks calibration routing).

## D4 Safety: only the authoritative sensor is required

**Decision**: `requiredSensorFault`/`skyLightFault` and `requiredDataAge` use the authoritative
copy only. A fault on the non-authoritative sensor never changes the verdict. A missing
authoritative reading stays unsafe exactly as a missing light sensor is today (the light sensor is
a required sensor even with the minimum-SQM rule off), with a reason naming it. Reason strings are
**literal per role**, so the device message catalogue (`tools/i18n/gen_device_catalog.py`) gets
fixed templates:

| Authoritative | Fault reason | Limit reason |
|---|---|---|
| SQM, SQM-L disabled (every existing device) | `Sensor fault: light sensor` (unchanged) | `SQM 18.21 < 19.50` (unchanged) |
| SQM, SQM-L enabled | `Sensor fault: SQM sensor` | `SQM 18.21 < 19.50` |
| SQM-L | `Sensor fault: SQM-L sensor` | `SQM-L 18.21 < 19.50` |

`SafetyInputs` gains `SkyRole skyRole` and `bool skyNamed` (two sensors enabled); the evaluator
picks the literal. Combined faults keep the existing "and IR sky sensor" forms per role.

**Rationale**: constitution I (doubt is unsafe, reasons state value and limit); SC-001 (no change
for one sensor); a `%s` sensor name would make the catalogue template match every reason.

**Alternatives**: require both sensors (a dead spare sensor would close the roof for no reason);
fall back to the other sensor (rejected by the owner, Clarifications Q1).

## D5 Config shape and migration

**Decision**:

- `sensor.i2c2SDA`, `sensor.i2c2SCL`: `uint8_t`, `255` = not wired (default on the classic ESP32
  build; board profiles set real pins later). Validated as a pair: both 255, or both valid GPIOs,
  different from each other and from every other configured pin (I2C-1, GPS, rain, wind),
  extending the existing conflict checks (FR-004).
- `sky`: `{ "sqmEnabled": true, "sqmlEnabled": false, "authoritative": "sqm", "showOther": true }`.
  `authoritative` is an enum (`SkyRole`) serialised as `"sqm"` / `"sqml"`. Validation:
  `sqmlEnabled` requires the I2C-2 pins; at least one of the two enabled is **not** required
  (a device may run with no sky sensor, as one with a missing TSL2591 does today).
- `skyAveragingL.windowSeconds`, `skyCalibrationL.*`: same fields, ranges and defaults as
  `skyAveraging` / `skyCalibration`, which stay the SQM's (FR-003).
- Missing keys take defaults (the `take()` reader already does this), so saved configs load
  unchanged (FR-014). No version bump or migration step is needed.
- Size: about +330 bytes of JSON, leaving about 1.9 KB of the 5100-byte limit at the measured
  long-string case. A native test pins the worst case under the limit (FR-015).

**Rationale**: parallel sections let the UI reuse one calibration component with a section-name
parameter and keep the existing keys untouched.

**Alternatives**: `sky.sensors[]` array (renames existing keys, breaks old configs and clients);
`skyL: {averaging, calibration}` nested (different shape from the SQM's, two code paths).

## D6 Readings document: authoritative fields + `skySensors`

**Decision**: `Readings::Snapshot` gains `SkySource source` (written as `sky.source`, only when the
`sky` group is written; `"sqm"` or `"sqml"`) and `SkySensorEntry skySensors[2]` with a `present`
flag (enabled). `skySensors` is written at the top level next to `sky` (same MQTT group), each
entry carrying the FR-008 fields with the "status only when not ok" rule of `Readings::write`;
`skySensors.deltaSqm` (rounded 2) only when both entries are `ok`. `sky.source` is always written,
including for a one-sensor device. The contract schemas use `additionalProperties: false`, so
`sky.source` and `skySensors` are added to them as **optional** properties; no existing property's
name, type or requirement changes, which is how SC-001's "contract tests pass unchanged" is read.

Document sizes grow by about 600 bytes serialised / 1.2 KB of pool. The MQTT state document
(3072) and the REST/WS document (4096) are raised to named constants in `Readings.h`
(`DOC_CAPACITY`), sized from a native test that builds the worst case (all groups ok, two sky
sensors) and asserts it fits with 20% headroom; the serialised MQTT payload stays under the 3072-byte
PubSubClient buffer (asserted in the same test).

**Rationale**: FR-007/FR-008; one serializer for REST, WS and MQTT (spec 013).

## D7 Home Assistant entities

**Decision**: three new entities in `ENTITIES[]` with a new group `SkyPair`: `sqm_wide`
("SQM (wide)", `value_json.skySensors.sqm.sqm`), `sqm_l` ("SQM-L (narrow)",
`value_json.skySensors.sqml.sqm`) and `sqm_delta` ("SQM-L − SQM", `value_json.skySensors.deltaSqm`).
`Groups` gains `skySqm`, `skySqml` (sky group on and that sensor enabled); `sqm_delta` needs both.
Disabled -> empty retained payload (removed, FR-008, US4 AC1). The existing `sqm`, `nelm`, `bortle`,
`illuminance` entities keep their ids and templates. Availability per entity reads that entry's
`status`.

## D8 Alpaca

**Decision**: `ObservingConditionsSnapshot` gains `SkyRole skyRole` and `bool skyNamed`;
`getSensorDescription` returns the existing strings for SQM when only the SQM is enabled, and
"TSL2591 SQM (wide)" / "TSL2591 SQM-L (narrow)" (and "..., calibrated SQM" for `skyquality`)
otherwise. `skyLight` source state follows the authoritative copy, so a disabled authoritative
sensor is NotImplemented and a faulted one is a driver error, exactly as a missing / failed
TSL2591 today. No interface version change; ConformU must stay at zero errors (SC-001).

## D9 BLE

**Decision**: the advertisement layout is unchanged (`sqm` = authoritative). The summary JSON adds
`"sqmSource": "sqm"|"sqml"` when `sqm` is present; the summary document grows from 256 to 320
bytes. `lib/BleLogic` is unchanged.

## D10 Alerts

**Decision**: new common template variables `sqm_wide`, `sqm_l`, `sqm_delta`, `sqm_source` in
`lib/AlertLogic/template-variables.json`, `Core::alertVars`, and
`web/src/components/settings/alertVariables.ts` (kept in step by the existing tests). A missing
value renders the way `{sqm}` does when the light sensor is down. Sensor fault/recovered events:
`AlertLogic::SENSOR_COUNT` 5 -> 6; slot 0 is the SQM ("Light sensor" when SQM-L is disabled, else
"SQM sensor"), slot 5 "SQM-L sensor" (present only when enabled). `{sqm}` / `{sqm_min}` stay the
authoritative value / the limit (FR-007).

## D11 Dark calibration route

**Decision**: `POST /api/sensors/tsl2591/calibrate-dark?sensor=sqm|sqml`. No parameter = `sqm`
(FR-009); an unknown value = 400; a disabled or never-begun sensor = 409 (existing "No light-sensor
readings to calibrate from"). Writes `skyCalibration.*` or `skyCalibrationL.*` only (US3 AC2) and
reconfigures that driver only. Query parameter rather than body: the route takes no body today and
`requireAuth` / OpenAPI already describe query parameters.

## D12 `/api/status`

**Decision**: `sensors.light` and `diagnostics.light` stay and follow the authoritative sensor
(same rule as the readings). New `skySensors: { sqm?: {...}, sqml?: {...} }` with, per enabled
sensor: `status`, `ageMs`, `detected`, `gain`, `integrationMs`, `saturated`, `nightMode`,
`sampleCount`, `windowSamples`, `darkVisibleOffset` (FR-008 "per-sensor health and diagnostics").
`detected` is "begun and answering", used by Settings and settings dependencies.

## D13 Settings dependencies (spec 020)

**Decision**: catalogue entries:

- `D-39` (dependency, setting): `sky.sqmlEnabled` depends on "the second I2C bus pins set",
  reason `i2c2-unset` ("Second I2C bus pins not set"), so Settings shows the switch blocked with a
  reason and a fix (FR-013). The saved config can never hold the SQM-L on without pins, so the
  device never reports it inactive; the fixture case that does is a draft (`draft: true`) the
  device refuses to save. The pin check is config-only (no `i2c2Configured` fact).
- `D-42` (constraint, feature): `sensor.i2c2SDA/SCL`, `sky.sqmlEnabled`: the pin rules and "the
  SQM-L needs the pins", with the device's messages (contracts/config.md). (Review fix: D-39 was
  first built as this constraint, which left the switch no way to show why it can't be used.)
- `D-40` (dependency, setting): `sky.authoritative`, `sky.showOther` depend on "SQM and SQM-L both
  on", reason `sky-single`.
- `D-41` (dependency, setting, runtime): `skyCalibrationL.enabled` depends on "SQM-L detected",
  reason `sqml-missing`.
- D-18 / D-29 keep `light-missing` with one sensor, meaning the authoritative sensor (fact
  `lightDetected` is the authoritative copy); D-29 stays on `skyCalibration.enabled` (the SQM's,
  fact `sqmDetected`). With both sensors on the reason names the sensor (FR-010): `sqm-missing`
  ("SQM not detected") or `sqml-missing` ("SQM-L not detected").
- `noDependency` gains `sensor.i2c2SDA`, `sensor.i2c2SCL`, `sky.sqmEnabled`,
  `skyAveragingL.windowSeconds`, `skyCalibrationL.<offsets>`.
`Deps::Facts` gains `i2c2Configured`, `sqmEnabled`, `sqmlEnabled`, `sqmDetected`, `sqmlDetected`.
`lib/SettingsDeps`, `web/src/lib/settingsDeps.ts`, `test/fixtures/settings-deps/cases.json` and
`docs/reference/settings-dependencies.md` move together (tools/settings-deps/check.py).

## D14 UI (DS rules)

**Decision**:

- Dashboard: `SkyHero` keeps its layout; a name chip (`Pill`, "SQM" / "SQM-L") joins the hero
  pills only when both sensors are enabled (DS-02, DS-21); the other reading and the difference are
  one `MetricTile` each in the existing `metric-grid` ("SQM-L 21.48", "Difference +0.25"),
  shown when `sky.showOther` and the other entry is `ok` (FR-012, DS-03). No new card styles.
  When the authoritative sensor is missing and the other reports, the existing fault/unavailable
  card shows the missing sensor's name and an action "Use SQM-L" (one `ActionButton`, saves
  `sky.authoritative`) (FR-010, DS-06/07).
- `LightCard` follows the authoritative sensor (unchanged). At-a-glance (spec 025): the expected
  sensor list becomes `light` (authoritative) + `sqml` when SQM-L is enabled and not authoritative
  (or `sqm` when SQM-L is authoritative and SQM enabled) once it has answered since boot, so each
  working or failed sensor has a tile and a disabled or never-detected one has none (constitution V,
  US1 AC3); contracts/ui.md has the full rule.
- Settings → Sensors: the Sky sensors card lists "SQM" and "SQM-L" rows with detected state and an
  enable `DepToggle` each, the I2C-2 SDA/SCL fields, then "Authoritative reading" (`SelectInput`)
  and "Show other reading" (`DepToggle`), both through settings dependencies (DS-04: reasons behind
  "?"). The Sky quality card is rendered once per enabled sensor (title "SQM" / "SQM-L"), the
  existing component parameterised by role (config sections, status entry, calibration call).
- Names: "SQM", "SQM-L" join `tools/i18n/glossary/en.json` (DS-27); new keys in
  `web/src/i18n/en.json` + context notes; 13 locales written by hand, then the record and checks.
- A mockup of the dashboard hero (both readings, missing-authoritative state) and Settings → Sensors
  is reviewed by the owner before the UI is built (the mockup was reviewed outside the repo)
  (constitution workflow, spec 026 FR-015).

## D15 Demo and contracts

**Decision**: the simulator's conditions gain `lightL` (present/failed flags, lux derived from the
SQM's with a darker offset, default +0.25 mag/arcsec²); `sensor_feed.cpp` fills `sky[Sqml]`; the
demo config enables SQM-L by default so the explorer shows both. `specs/016-demo-device-emulation/
contracts/schemas/{readings,status,config,settings-effective}.schema.json` gain the new fields;
`tools/api/operations.json` documents the `sensor` query parameter; `docs/api/openapi.json` is
regenerated (`tools/api/openapi.py`, then `--check`); the WASM core is rebuilt (`tools/demo-core/
build.sh`) and `SOURCE_HASH` updated.

## D16 Budgets

**Decision**: measured in the tasks: firmware size of both builds before/after (expect < 6 KB:
driver changes, a second `Adafruit_TSL2591`, new JSON fields); heap +~3.3 KB for the second
sensor's sample arrays (allocated only when SQM-L is enabled: the arrays move behind a
`std::unique_ptr` allocated in `begin()`); gzipped UI growth under 10 KB (SIZE-02); main config
JSON under 5100 bytes (native test). Device measurement of heap/stack (constitution IV) is done on
the **spare** device only, recorded in the PR.
