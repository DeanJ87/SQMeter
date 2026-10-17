# Tasks: Two sky sensors (SQM and SQM-L)

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: required by constitution III (native Unity tests for `lib/*` logic, Vitest for UI logic,
a failing test before each fix). Test tasks come before the code they cover and must fail first.

**Format**: `- [ ] T### [P?] [US?] Description with file path`. `[P]` = different files, no
dependency on an unfinished task.

## Phase 1: Setup

- [X] T001 Record the baselines in `specs/030-dual-sky-sensors/budgets.md`: firmware size of `pio run -e esp32dev` and `-e esp32dev-ble`, gzipped UI JS+CSS (`web` build), default and long-string main config JSON size, worst-case readings document size (research "What exists today")
- [X] T002 [P] Add `SkyRole { Sqm = 0, Sqml = 1 }`, `SKY_ROLE_COUNT = 2`, `skyRoleName()` (`"sqm"`/`"sqml"`), `skyRoleLabel()` (`"SQM"`/`"SQM-L"`) and `parseSkyRole()` (false on anything else) in lib/SensorTypes/include/SensorTypes.h, with tests in test/test_sky_logic/test_sky_role.cpp

## Phase 2: Foundational (blocks every story)

### Config model (research D5, contracts/config.md)

- [X] T003 [P] Native tests first in test/test_config_model/: defaults (`sensor.i2c2SDA`/`i2c2SCL` = 255 "unset", `sky.sqmEnabled` true, `sky.sqmlEnabled` false, `sky.authoritative` `"sqm"`, `sky.showOther` true, `skyAveragingL.windowSeconds` 90, `skyCalibrationL` like `skyCalibration`); JSON round trip; validation messages from contracts/config.md (`i2c2` "both 255, or both valid GPIOs and different"; not used by I2C-1, GPS, rain or wind; `sqmlEnabled` needs I2C-2 pins; `authoritative` must be `sqm` or `sqml`; `skyAveragingL.windowSeconds` 10..300; `skyCalibrationL.sqmOffset` -5..5; dark offsets >= 0 and finite); worst-case main JSON (long strings, both sensors configured) < `Config::MAX_PERSISTED_JSON_BYTES` (5100)
- [X] T004 [P] Migration test first in test/test_config_migration/: a pre-030 saved config fixture (test/fixtures/config-releases/) loads with every existing value kept and the new keys at their defaults (FR-014)
- [X] T005 Add `SkyConfig sky`, `skyAveragingL`, `skyCalibrationL`, `sensor.i2c2SDA/i2c2SCL` and the helpers `averaging(SkyRole)`, `calibration(SkyRole)`, `skyEnabled(SkyRole)` in lib/ConfigModel/include/Config.h; defaults in lib/ConfigModel/src/ConfigModel.cpp
- [X] T006 JSON in/out for the new keys in lib/ConfigModel/src/ConfigJsonIn.cpp and lib/ConfigModel/src/ConfigJsonOut.cpp (`authoritative` read with `parseSkyRole`, rejected when unknown)
- [X] T007 Validation in lib/ConfigModel/src/ConfigValidate.cpp: I2C-2 pair rule, I2C-2 in the pin-conflict checks (255 = unused) including `validateWindPins`, `sqmlEnabled` needs pins, SQM-L averaging/calibration ranges, messages exactly as contracts/config.md; T003/T004 pass
- [X] T008 [P] Range fixture entries for the new keys in test/fixtures/config-ranges.json and test/fixtures/settings-deps/default-config.json (checked by test/test_config_ranges)

### Read cycle (research D2, data-model SkyReadCycle)

- [X] T009 [P] Native tests first in test/test_sky_logic/test_sky_read_cycle.cpp: start when due; collect only after `integrationMs + MARGIN_MS`; success resets failures; 3 consecutive failures -> Absent with 5 s back-off doubling to a 60 s cap; never-begun starts Absent with retry at once; replug (begin ok) -> Idle; millis wrap
- [X] T010 `SkyReadCycle` in lib/SkyLogic/include/calculations/SkyReadCycle.h and lib/SkyLogic/src/SkyReadCycle.cpp (no Arduino headers); T009 passes

### Per-sensor snapshot and authoritative copy (research D3)

- [X] T011 Native tests first in test/test_device_core/: with `sky.authoritative` = sqm/sqml and each role enabled/disabled/faulted, `Core::derive` sets `tsl`, `tslDiagnostics`, `tslInitialized`, `tslLastUpdate`, `sky` and `skySource` from the authoritative role only; a disabled or faulted authoritative role gives the empty not-ok copy and never the other role's values (FR-010); one-sensor config gives exactly today's copy
- [X] T012 Add `SkySensorSnapshot skySensors[SKY_ROLE_COUNT]` and `SkyRole skySource` to `SensorSnapshot` and fill the authoritative copy and per-role `metrics` in `Core::derive` in lib/DeviceCore/include/DeviceCore.h and lib/DeviceCore/src/DeviceCore.cpp; T011 passes

**Checkpoint**: config and core compile natively; existing native tests unchanged and green.

## Phase 3: User Story 1 - Both readings on one device (P1) MVP

**Goal**: two TSL2591s read independently; both readings in the API and on the dashboard.
**Independent test**: with SQM-L enabled, `/api/sensors` has `skySensors.sqm` and `.sqml` with
their own status; covering one changes only that entry (SC-002).

### Tests

- [X] T013 [P] [US1] Native tests first in test/test_readings/: `skySensors` entries only for enabled roles, every FR-008 field and rounding, status-only form when not ok, `deltaSqm` only when both ok, `sky.source` always with the `sky` group, one-sensor golden document unchanged apart from `sky.source`; worst-case document fits `Readings::DOC_CAPACITY` with 20% headroom and its payload is under 3072 bytes
- [X] T014 [P] [US1] Native tests first in test/test_device_core/: `buildReadings` per-role statuses (ok/missing/error/stale per role, stale after `readIntervalMs + SENSOR_STALE_GRACE_MS`), `/api/status` `skySensors` health + diagnostics per enabled role (contracts/status-and-routes.md), `sensors.light`/`diagnostics.light` follow the authoritative role

### Implementation

- [X] T015 [US1] `SkySource source`, `SkySensorEntry skySensors[2]`, `deltaValid/deltaSqm`, `DOC_CAPACITY` and their writing in lib/Readings/include/Readings.h and lib/Readings/src/Readings.cpp; T013 passes
- [X] T016 [US1] Fill `skySensors` and `source` in `readSkyGroups`, add `skySensors` to `writeSensorHealth`/`writeDiagnostics` in lib/DeviceCore/src/DeviceCore.cpp; T014 passes
- [X] T017 [US1] Driver: constructor takes `TwoWire &` and `SkyRole`; `begin()` uses `Adafruit_TSL2591::begin(&bus)`; sample buffers behind `std::unique_ptr` allocated in `begin()`; start/collect reads with direct `TwoWire` CHAN0/CHAN1 transactions that report `ReadError` on `endTransmission() != 0` or `requestFrom() != 4`; `SkyReadCycle` drives start, collect and re-begin, in include/sensors/TSL2591Sensor.h and src/sensors/TSL2591Sensor.cpp
- [X] T018 [US1] src/main.cpp: `Wire1.begin(i2c2SDA, i2c2SCL)` only when both pins set and SQM-L enabled; two `TSL2591Sensor` objects (SQM on `Wire`, SQM-L on `Wire1`), each begun only when enabled; overlapped non-blocking `tick()` for both in `loop()` replacing the blocking 650 ms read; per-role `configureSkyMeasurement`
- [X] T019 [US1] `WebServer::Sensors` holds both sky sensors; `refreshSensorSnapshot` copies both roles' reading, diagnostics, enabled, initialized and last update; config save reconfigures each role's averaging/calibration; readings documents use `Readings::DOC_CAPACITY`, in include/WebServer.h, src/WebServer.cpp, src/WebServerApi.cpp, src/WebServerStatus.cpp
- [X] T020 [US1] Both firmware builds pass with `-Werror` (`pio run -e esp32dev`, `-e esp32dev-ble`); record sizes in specs/030-dual-sky-sensors/budgets.md
- [X] T021 [P] [US1] Web types and mocks: `SkyReading.source`, `SensorData.skySensors`, `Status.skySensors` in web/src/types/readings.ts and web/src/types/status.ts; MSW mocks with both sensors in web/src/mocks/handlers.ts
- [ ] T022 [US1] Mockup of the Sky quality card states and Settings → Sensors from contracts/ui.md in specs/030-dual-sky-sensors/mockups/ (390 px and 1280 px); owner review recorded in the mockups README before T023-T025 and T035/T042 are built
- [ ] T023 [US1] Vitest first, then `SkyHero` name pill (both enabled), other-reading and Difference `MetricTile`s (shown with `sky.showOther` and the other entry ok; amber tone above 1.5 mag/arcsec²), MiniSpark restart on source change, in web/src/components/dashboard/skyCards.tsx and web/src/components/__tests__/
- [ ] T024 [US1] Vitest first, then a glance tile per enabled sky sensor; none for a disabled one or one that has not answered since boot (`missing`, never detected), except the authoritative sensor, whose state the Sky card reports (FR-010); a sensor that answered and then failed keeps its tile with fault text naming it ("SQM-L not responding"), in web/src/dashboard/glance.ts and web/src/dashboard/__tests__/
- [ ] T025 [US1] Settings Sky sensors card: "SQM" and "SQM-L" rows with detected badge and enable `DepToggle`, "I2C-2 SDA"/"I2C-2 SCL" fields (empty = 255), in web/src/components/settings/SensorsSkyCards.tsx; Zod and defaults in web/src/validation/configSchema.ts and web/src/components/settings/defaults.ts; payload in web/src/components/settings/payload.ts; Vitest in web/src/components/settings/__tests__/
- [X] T026 [P] [US1] Sensor names "SQM", "SQM-L" in web/src/lib/sensorNames.ts and tools/i18n/glossary/en.json (DS-27)
- [ ] T027 [US1] Dashboard inventory entries for `readings.skySensors.**`, `readings.sky.source`, `status.skySensors.**` (shown or not-shown with reason) in web/src/dashboard/inventory.json; `inventory:` ids in web/tests/dashboard.spec.ts; `tools/dashboard/check.py` passes

**Checkpoint**: US1 works with SQM authoritative (default) and both readings visible.

## Phase 4: User Story 2 - Choose which reading counts (P1)

**Goal**: `sky.authoritative` moves every single-value consumer, with no fallback.
**Independent test**: switch SQM <-> SQM-L; `sky.source`, Alpaca `skyquality` +
`SensorDescription`, BLE summary `sqmSource`, safety reasons all change (SC-003).

### Tests

- [X] T028 [P] [US2] Native tests first in test/test_alpaca_safety/: literal reasons per research D4 table (`Sensor fault: light sensor` unchanged with one sensor; `Sensor fault: SQM sensor`; `Sensor fault: SQM-L sensor`; the "and IR sky sensor" forms; `SQM %.2f < %.2f`; `SQM-L %.2f < %.2f`); authoritative faulted with the other ok -> unsafe and the limit rule not evaluated; non-authoritative fault -> verdict unchanged; `requiredDataAge` uses the authoritative role
- [X] T029 [P] [US2] Native tests first in test/test_alpaca_description/ and test/test_alpaca_logic/: `SensorDescription` per contracts/integrations.md table; `skyquality`/`skybrightness` NotImplemented when the authoritative role is disabled, no-data when faulted; values follow the authoritative role
- [X] T030 [P] [US2] Native tests first in test/test_settings_deps/ and test/fixtures/settings-deps/cases.json: D-39 (`sky.sqmlEnabled` needs I2C-2 pins, reason `i2c2-unset`), D-40 (`sky.authoritative`, `sky.showOther` need both on, reason `sky-single`), D-41 (`skyCalibrationL.enabled` needs SQM-L detected, reason `sqml-missing`), D-18/D-29 facts

### Implementation

- [X] T031 [US2] `SafetyInputs::skyRole/skyNamed` and literal reason selection in lib/AlpacaLogic/include/SafetyEvaluator.h and lib/AlpacaLogic/src/SafetyEvaluator.cpp; set them in `Core::safetyInputs` (lib/DeviceCore/src/DeviceCore.cpp); T028 passes
- [X] T032 [US2] `ObservingConditionsSnapshot::skyRole/skyNamed` and per-role descriptions in lib/AlpacaLogic/include/ObservingConditionsMapper.h and lib/AlpacaLogic/src/ObservingConditionsMapper.cpp; set in `Core::observingConditions`; T029 passes
- [X] T033 [US2] Catalogue entries D-39..D-41, reasons `i2c2-unset`, `sky-single`, `sqml-missing`, new keys in `noDependency` in lib/SettingsDeps/catalogue.json; `Deps::Facts` + `i2c2Configured`, `sqmEnabled`, `sqmlEnabled`, `sqmDetected`, `sqmlDetected` in lib/SettingsDeps/include/SettingsDeps.h and its src; `Core::sensorFacts`; mirror in web/src/lib/settingsDeps.ts and web/src/components/settings/depLabels.ts; `tools/settings-deps/check.py` passes; T030 passes
- [X] T034 [US2] BLE summary `sqmSource` (document 256 -> 320 bytes) in src/WebServerAlerts.cpp `updateBle`; advertisement unchanged
- [ ] T035 [US2] Vitest first, then Settings "Authoritative reading" `SelectInput` and "Show other reading" `DepToggle` (D-40), and the Note + "Use SQM"/"Use SQM-L" action when the saved authoritative sensor is off, in web/src/components/settings/SensorsSkyCards.tsx
- [ ] T036 [US2] Vitest first, then the Sky quality unavailable/fault card naming the missing authoritative sensor with a "Use <other>" `ActionButton` that saves `sky.authoritative` (FR-010), in web/src/components/dashboard/skyCards.tsx and web/src/components/dashboard/cards.tsx
- [X] T037 [US2] Device message catalogue for the new reasons and validation messages: `python3 tools/i18n/gen_device_catalog.py` then `--check` (web/src/i18n/en.json `device.*`)

**Checkpoint**: US1 + US2 deliver the P1 scope.

## Phase 5: User Story 3 - Calibrate each sensor on its own (P2)

**Goal**: averaging, offset and dark calibration per sensor.
**Independent test**: dark-calibrate SQM-L; only `skyCalibrationL` changes (US3 AC2).

- [X] T038 [P] [US3] Native test first in test/test_device_core/: a pure helper `Core::applyDarkCalibration(Config &, SkyRole, const TSL2591Diagnostics &, int64_t at)` writes only that role's calibration section and leaves the other byte-identical
- [X] T039 [US3] `applyDarkCalibration` in lib/DeviceCore/src/DeviceCore.cpp (declared in lib/DeviceCore/include/DeviceCore.h); T038 passes
- [X] T040 [US3] Route `POST /api/sensors/tsl2591/calibrate-dark?sensor=sqm|sqml` in src/WebServerApi.cpp: no parameter = SQM; unknown = 400 "Unknown sky sensor"; disabled/undetected/no samples = 409 (existing message); success body adds `"sensor"`; reconfigures only that driver; `requireAuth` kept
- [X] T041 [P] [US3] `useDarkCalibration(sensor)` posting `?sensor=` in web/src/hooks/useSensorActions.ts with a Vitest case
- [ ] T042 [US3] `SkyQualityCard` parameterised by role (config sections `skyAveraging`/`skyCalibration` or `skyAveragingL`/`skyCalibrationL`, status `skySensors.<role>`, calibration call), rendered per enabled sensor with titles "SQM"/"SQM-L" (single "Sky quality" card when only SQM is enabled), in web/src/components/settings/SensorsSkyCards.tsx and web/src/components/settings/SensorsTab.tsx; Vitest
- [X] T043 [P] [US3] OpenAPI: optional `sensor` query parameter (enum `sqm`, `sqml`) and the 400 in tools/api/operations.json; regenerate docs/api/openapi.json (`tools/api/openapi.py`, then `--check`)

## Phase 6: User Story 4 - Integrations see both (P2)

**Goal**: HA entities, alert variables and sensor events for both sensors.
**Independent test**: HA creates SQM (wide), SQM-L (narrow), SQM-L − SQM beside `sqm`; `{sqm_l}`
renders the SQM-L value.

- [ ] T044 [P] [US4] Native tests first in test/test_readings/: discovery publishes `sqm_wide`, `sqm_l`, `sqm_delta` with names, templates, units and availability from contracts/integrations.md when enabled, empty retained payloads when disabled (`sqm_delta` needs both); existing `sqm`, `nelm`, `bortle`, `illuminance` payloads byte-identical
- [ ] T045 [P] [US4] Native tests first in test/test_alert_variables/ and test/test_alert_logic/: `{sqm_wide}`, `{sqm_l}`, `{sqm_delta}` (signed, 2 decimals), `{sqm_source}` (`SQM`/`SQM-L`), missing values like `{sqm}`; sensor slots ("Light sensor" with SQM-L disabled, else "SQM sensor"; "SQM-L sensor" only when enabled) for `sensor_fault`/`sensor_recovered`
- [ ] T046 [US4] `Groups::skySqm/skySqml`, group `SkyPair` and the three entities in lib/Readings/src/Readings.cpp; set the groups from config in src/WebServerStatus.cpp `publishDiscovery` path; T044 passes
- [ ] T047 [US4] `Alerts::SENSOR_COUNT` 6 in lib/AlertLogic/include/AlertEngine.h; slot names and the new variables in lib/DeviceCore/src/DeviceAlerts.cpp; lib/AlertLogic/template-variables.json; web/src/components/settings/alertVariables.ts with descriptions; T045 passes

## Phase 7: Polish and cross-cutting

### Demo and contracts (FR-011)

- [ ] T048 [P] Contract schemas: `sky.source` (optional enum) and `skySensors` (optional) in specs/016-demo-device-emulation/contracts/schemas/readings.schema.json; `skySensors` in status.schema.json; new config keys in config.schema.json and settings-effective.schema.json; no existing property changed
- [ ] T049 Demo: simulator `lightL` condition (present/failed, SQM-L ~0.25 mag/arcsec² darker), SQM-L enabled in the demo config, demo panel control to fail it, in web/src/demo/conditions.ts, web/src/demo/simulator.ts, web/src/demo/device.ts, web/src/demo/presets.ts; `sensor_feed.cpp` fills `skySensors[Sqml]` in tools/demo-core/sensor_feed.cpp
- [ ] T050 Rebuild the WASM core with tools/demo-core/build.sh (updates web/src/demo/core/* and SOURCE_HASH); contract check (`tools/contract-check.py`) passes for device mocks and demo

### Strings (FR-017)

- [ ] T051 Every new UI string in web/src/i18n/en.json with a context note in web/src/i18n/en.context.json; translations written for all 13 locales in web/src/i18n/locales/*.json in each file's register; `python3 tools/i18n/translate.py --all --record`, `node tools/i18n/check.mjs`, `python3 tools/i18n/gen_device_catalog.py --check` pass (no paid API)

### Docs (FR-016, constitution VII)

- [ ] T052 [P] docs/reference/sky-quality.md: what SQM and SQM-L measure, why SQM-L usually reads darker, the authoritative reading and no-fallback rule; docs/development/adr-001-sky-optics-and-averaging.md note on the narrow lens
- [ ] T053 [P] docs/user-guide/configuration.md (new keys, defaults, validation), docs/reference/settings-dependencies.md (D-39..D-41, regenerated where generated)
- [ ] T054 [P] docs/api/rest.md and docs/api/websocket.md (`sky.source`, `skySensors`, `/api/status` `skySensors`, calibrate-dark `?sensor=`)
- [ ] T055 [P] docs/user-guide/mqtt.md (state `skySensors`, HA entities), docs/user-guide/alpaca.md (source, descriptions, safety reasons), docs/user-guide/ble.md (`sqmSource`), docs/user-guide/alerts.md (new variables, sensor names)
- [ ] T056 [P] Hardware: the I2C-2 port and second sensor in docs/hardware/overview.md, docs/hardware/assembly.md and docs/getting-started/hardware.md; Settings paths match en.json labels (DS-28); `python3 -m mkdocs build --strict` where mkdocs is installed

### Budgets and gates (FR-015, constitution IV, VIII)

- [ ] T057 Measure after: firmware size both builds (SIZE-04), gzipped UI delta <= 10 KB (SIZE-02), config and readings worst cases (tests), update specs/030-dual-sky-sensors/budgets.md; device heap and `stackFree` on the spare device only, noted for the PR
- [ ] T058 Full gate: `pio test -e native`, both firmware builds, ConformU against tools/alpaca-sim with zero errors, issues and configuration alerts (CI workflow, or Docker locally) with one and with two sky sensors configured (SC-001), `tools/quality/check.py` (no new findings), `tools/api/openapi.py --check`, web `vitest run`, `tsc --noEmit`, `eslint src`, i18n checks
- [ ] T059 Run quickstart.md sections 1 and 2 and record results; changelog entry in CHANGELOG.md
- [ ] T060 Converge (`speckit-converge`) until it reports converged

## Dependencies

- Phase 1 -> Phase 2 -> stories. T005 before T006/T007; T010 before T017; T012 before T015/T016.
- US1 (Phase 3) is the MVP and is required by US2 (T031-T036 consume the per-role snapshot and UI
  from T015-T025).
- US3 needs T012 (per-role config) and T017-T019 (per-role drivers); independent of US2/US4.
- US4 needs T015 (skySensors) and T012; independent of US2/US3.
- T022 (mockup review) gates T023-T025, T035, T036, T042.
- Polish after the stories it documents; T050 after every `lib/*` change; T060 last.

## Parallel examples

- Phase 2: T003, T004, T008, T009 together (different test dirs/fixtures).
- US1: T013 and T014 together; T021 and T026 alongside T017-T019.
- US2: T028, T029, T030 together.
- US4: T044 and T045 together.
- Polish: T048 and T052-T056 together.

## Implementation strategy

1. MVP = Phases 1-3 (US1) with SQM authoritative: both readings visible, nothing changes for
   one-sensor devices.
2. Add US2 (authoritative choice, safety naming) to complete the P1 scope.
3. US3 and US4 in either order.
4. Polish: demo, schemas, translations, docs, budgets, convergence.
