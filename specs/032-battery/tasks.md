# Tasks: Li-ion battery and fuel gauge

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: required by constitution III (native Unity tests for `lib/*` logic, Vitest for UI logic).
Test tasks come before the code they cover and must fail first.

**Prerequisite**: spec 031 implemented on this branch (OLED Battery page view group, display
`forcedOff` input, display button pin check).

**Format**: `- [ ] T### [P?] [US?] Description with file path`. `[P]` = different files, no
dependency on an unfinished task.

## Phase 1: Setup

- [X] T001 Record the baselines (after spec 031) in specs/032-battery/budgets.md: firmware size of both builds, static RAM, gzipped UI JS+CSS, default and long-string main config JSON, alerts Main/Client part sizes, readings worst case (`test_worst_case_document_fits`), `/api/status` worst case
- [X] T002 Create lib/BatteryLogic/ (include/, src/, like lib/WindLogic); add `-Ilib/BatteryLogic/include` to `[env:native]` in platformio.ini and `lib/BatteryLogic/src/*.cpp` to SOURCES in tools/demo-core/build.sh
- [X] T003 [P] `ChargeState` (Discharging, Charging, Charged, Fault, NoBattery, Unknown) with `chargeStateName()` (`discharging`, `charging`, `charged`, `fault`, `no-battery`), `ChargeFault` (None, Temperature), `PowerState` (Normal, Saving, ShutdownPending) in lib/BatteryLogic/include/BatteryLogic.h; `BatteryReading` (raw registers + decoded values) in lib/SensorTypes/include/SensorTypes.h; tests first in test/test_battery_logic/test_types.cpp

## Phase 2: Foundational (blocks every story)

### Config model (research D6, D8, contracts/config.md)

- [X] T004 [P] Native tests first in test/test_config_model/: `battery.*`, `alpaca.battery*`, `mqtt.publish.battery` and the six battery event defaults; JSON round trip; every validation message in contracts/config.md (ranges, low > critical > shutdown ordering, capacity 0 or 500..50000, charger pins all-or-none, distinct pins, clash with I2C-1/I2C-2, GPS, rain, wind and `display.buttonPin`, PG RTC-capable); `alertsToJson(…, AlertsPart::Battery)` holds exactly the six events and Main/Client parts are unchanged byte for byte; worst cases: main JSON (long strings, both sky sensors, display, battery) < 5,100 B, battery part with every template at its limit ≤ 3,900 B
- [X] T005 [P] Migration test first in test/test_config_migration/: a pre-032 saved config (fixture in test/fixtures/config-releases/) loads with every value kept and battery defaults; a stored config without `alertsBatt` gets the six default events
- [X] T006 Add `BatteryConfig battery`, `AlpacaConfig` battery fields, `MQTTConfig::Publish::battery`, six `AlertsConfig::EventSetting`s and `AlertsPart::Battery` in lib/ConfigModel/include/Config.h; defaults in lib/ConfigModel/src/ConfigModel.cpp; `EVENT_COUNT`/part boundaries and `eventSettingsOf` in lib/ConfigModel/src/ConfigInternal.h
- [X] T007 JSON in/out in lib/ConfigModel/src/ConfigJsonIn.cpp and lib/ConfigModel/src/ConfigJsonOut.cpp (new section, alpaca/mqtt keys, Battery alerts part)
- [X] T008 Validation in lib/ConfigModel/src/ConfigValidate.cpp: messages exactly as contracts/config.md; battery pins join the pin-conflict set used by `validateWindPins` and the display button check (spec 031); all three alerts parts ≤ `MAX_ALERTS_JSON_BYTES`; T004 passes
- [X] T009 NVS key `alertsBatt`: save the Battery part and splice it in on load like the Client part in src/ConfigStore.cpp; T005 passes natively for the splice logic it shares
- [X] T010 [P] Range and default fixture entries in test/fixtures/config-ranges.json and test/fixtures/settings-deps/default-config.json (test/test_config_ranges)

### Settings dependencies (research D11)

- [X] T011 Native tests first in test/test_settings_deps/ and cases in test/fixtures/settings-deps/cases.json for D-46..D-51 (D-47 fail-safe; D-49 mqtt-off and battery-missing; D-50 like D-08; D-51 messages); `alpaca.batteryMinPercent`, `alpaca.batteryAlert` in `noDependency`
- [X] T012 Catalogue entries D-46..D-51 and reason `battery-missing` ("No battery gauge detected", fix `device#battery`) in lib/SettingsDeps/catalogue.json; `Facts.batteryDetected` and entries in lib/SettingsDeps/include/SettingsDeps.h and lib/SettingsDeps/src/SettingsDeps.cpp; mirror in web/src/lib/settingsDeps.ts; `tools/settings-deps/check.py` and T011 pass

### Snapshot plumbing

- [X] T013 Add `BatterySnapshot battery` to `SensorSnapshot` (`battery.enabled` off → gauge not read and reported like no gauge, research D8) (present, initialized, lastUpdate, reading, estimating, charger output, estimate, policy state, gauge alert level) in lib/DeviceCore/include/DeviceCore.h; `Core::derive` fills the decoded/estimate fields from `lib/BatteryLogic`

**Checkpoint**: config, deps and snapshot compile natively; existing native tests unchanged and green.

## Phase 3: User Story 1 - See how much battery is left (P1) MVP

**Goal**: with a gauge, the battery object appears everywhere and the dashboard shows a Battery card;
without one, only `battery.present: false`.
**Independent test**: quickstart sections 3 and 4.1-4.2.

### Tests

- [X] T014 [P] [US1] Native tests first in test/test_battery_logic/test_gauge.cpp: register decoding (VCELL 78.125 µV/LSB, SOC 1/256 %, CRATE 0.208 %/h signed), percent clamp, VERSION 0x001x detection, RI → estimating for 2 s, SOC 0 with VCELL > 3.5 V → estimating, rate EMA (10 min) valid after 60 s
- [X] T015 [P] [US1] Native tests first in test/test_battery_logic/test_charger.cpp: every row of the truth table in research D3 incl. fault → `temperature`; 2 s stability; toggling > 6 s → no-battery; pins unset → state from the smoothed rate with `usbPower` unknown; change reported within 5 s (SC-002)
- [X] T016 [P] [US1] Native tests first in test/test_battery_logic/test_estimate.cpp: time to empty/full from percent and rate; null when estimating, |rate| < 0.5 %/h, or state not discharging/charging; cap 5,940 min; capacity lower bound on time to full at 833 mA; `currentMa` only with capacity
- [X] T017 [P] [US1] Native tests first in test/test_readings/: `battery` group per contracts/readings.md (`{"present": false}` only without a gauge; full object; status-only when not ok; nulls); worst-case document with every group ok, both sky sensors and the battery fits `DOC_CAPACITY` with 20% headroom and serialises under 3,072 B; one-gauge-less golden document unchanged apart from `battery.present`
- [X] T018 [P] [US1] Native tests first in test/test_device_core/: `buildReadings` battery status (ok/missing/error/stale after `readIntervalMs + SENSOR_STALE_GRACE_MS`, estimating), monitoring off → `{"present": false}`, gauge missing with charger pins set still gives `state`/`usbPower` (pack removed on USB → `no-battery`), `/api/status` `battery`, `sensors.battery`, `diagnostics.battery` (contracts/readings.md), `Facts.batteryDetected`; `test_status_document_fits` passes with the battery worst case (raise `STATUS_DOC_CAPACITY` only if needed)

### Implementation

- [X] T019 [US1] `GaugeDecoder`, `ChargerDecoder`, `Estimate` in lib/BatteryLogic/include/BatteryLogic.h and lib/BatteryLogic/src/{Gauge,Charger,Estimate}.cpp; T014-T016 pass
- [X] T020 [US1] `Readings::Battery`, `Groups.battery`, writing, `DOC_CAPACITY` in lib/Readings/include/Readings.h and lib/Readings/src/Readings.cpp; T017 passes
- [X] T021 [US1] Readings/status/diagnostics/facts for the battery in lib/DeviceCore/src/DeviceCore.cpp (or a new lib/DeviceCore/src/DeviceBattery.cpp); T018 passes
- [X] T022 [US1] Gauge driver (with `battery.enabled` off: one presence probe at boot for `Facts.batteryDetected`, then no reads and no pins touched): detect (ACK at 0x36 + VERSION), read VCELL/SOC/CRATE/STATUS every 5 s, clear RI, charger pins with `INPUT_PULLUP` when set, ALRT level for diagnostics, in include/sensors/MAX17048Sensor.h and src/sensors/MAX17048Sensor.cpp
- [X] T023 [US1] src/main.cpp: create the gauge after `setupSensors()`, tick it in `loop()`; `WebServer::Sensors` gains the gauge and `refreshSensorSnapshot` copies its reading; status and readings use it, in include/WebServer.h, src/WebServer.cpp, src/WebServerStatus.cpp
- [X] T024 [US1] Spec 031 Battery page: fill the `battery` view group (percent, state text, availability = present) in lib/DeviceCore/src/DeviceDisplay.cpp with a test in test/test_device_core/
- [X] T025 [US1] Both firmware builds pass with `-Werror`; record sizes in specs/032-battery/budgets.md
- [X] T026 [P] [US1] Web types and mocks: `Battery` in web/src/types/readings.ts and web/src/types/status.ts, MSW mocks with and without a gauge in web/src/mocks/
- [X] T027 [US1] Mockup of the dashboard Battery card states, glance tile, System rows, Settings Battery card, Safety battery rule row and Alerts battery group from contracts/ui.md in specs/032-battery/mockups/ (390 px and 1280 px) with a README; owner review recorded before T028-T030, T040, T041 and T054 are built (round 1: wireframes in `mockups/README.md`; the owner's review is still to be recorded there)
- [ ] T028 [US1] Vitest first, then the dashboard Battery card per contracts/ui.md (only with `battery.present`; states, "--", "> 99 h", Power saving pill) in web/src/components/dashboard/ (new batteryCard.tsx or cards.tsx) and web/src/components/__tests__/; card order in web/src/components/dashboard/order.ts
- [ ] T029 [US1] Vitest first, then the glance battery tile (below low warn; below critical, fault or shutdown pending bad; none otherwise or without a gauge) in web/src/dashboard/glance.ts and web/src/dashboard/__tests__/
- [ ] T030 [US1] System page battery rows in the Sensors card in web/src/components/System.tsx with a Vitest in web/src/components/__tests__/System.test.tsx
- [ ] T031 [US1] Dashboard inventory entries for every new readings/status field and D-46..D-51 in web/src/dashboard/inventory.json; `inventory:` ids in web/tests/dashboard.spec.ts; `tools/dashboard/check.py` passes

**Checkpoint**: US1 works on a board with a gauge; boards without one are unchanged (SC-005).

## Phase 4: User Story 2 - Get warned before it runs out (P1)

**Goal**: low/critical alerts once per crossing; optional fail-safe battery safety rule with alert and
phone alarm options.
**Independent test**: quickstart section 5.

### Tests

- [X] T032 [P] [US2] Native tests first in test/test_alpaca_safety/: battery rule off → no battery reason; on with no gauge/estimating/stale → `Battery data missing` (bit 15); below limit → `Battery 8% < 10%` (bit 14, integer percent); checked even when every other sensor is stale and alongside rain; charging below the limit stays unsafe; shutdown pending → `Shutting down: battery 4% < 5%` (bit 16) with the rule off
- [X] T033 [P] [US2] Native tests first in test/test_alert_logic/: low and critical fire once below their thresholds, not again until percent ≥ threshold + 5, then again on the next crossing; invalid/estimating data never fires; charge complete on entering charged; charge fault on entering fault, once; cooldown applies to these; a full simulated discharge gives exactly one low alert (SC-003)
- [X] T034 [P] [US2] Native tests first in test/test_device_core/: `safetyInputs`/`safetyThresholds` battery mapping; `runAlerts` gives battery events their configured level, Wake level adds bit 14 to `alarmFlags`; the Battery rule alert at Urgent when `alpaca.batteryAlert` and bit 14/15 newly appear, stacked with the unsafe alert; `batteryPhoneAlarm` requests the BLE alarm with that bit; `eventSettingFor` and `sampleAlert` cover the six events

### Implementation

- [X] T035 [US2] Battery thresholds/inputs, `UnsafeBatteryLow`/`UnsafeBatteryData`/`UnsafeShutdown` and `checkBattery` (independent of freshness) in lib/AlpacaLogic/include/SafetyEvaluator.h and lib/AlpacaLogic/src/SafetyEvaluator.cpp; T032 passes
- [X] T036 [US2] AlertTypes `BatteryLow`, `BatteryCritical`, `BatteryShutdown`, `PowerRestored`, `ChargeComplete`, `ChargeFault`, `BatteryRule` (names table, `ALERT_TYPE_COUNT`), `AlertInputs.battery`, `AlertRules` battery fields and `updateBattery` in lib/AlertLogic/include/AlertEngine.h and lib/AlertLogic/src/AlertEngine.cpp; T033 passes
- [X] T037 [US2] DeviceCore: safety mapping, alert inputs/rules, event settings, Wake flags, rule alert and phone-alarm request, sample alerts in lib/DeviceCore/src/DeviceCore.cpp, lib/DeviceCore/src/DeviceAlerts.cpp and lib/DeviceCore/include/DeviceCore.h; T034 passes
- [X] T038 [US2] Firmware: raise the BLE alarm for the rule's phone-alarm request in src/WebServerAlerts.cpp (`sendAlerts`/`updateBle`), through the existing schedule
- [X] T039 [US2] ConformU against tools/alpaca-sim (CI, or Docker locally) with zero errors, issues and configuration alerts, with the battery rule off and on (round 1: ConformU v4.5.0 in Docker, `one` and the new `battery` simulator mode, both devices, conformance and Alpaca protocol: no errors, issues or information alerts; CI runs the `battery` mode too)
- [ ] T040 [US2] Vitest first, then the Safety tab battery rule row (toggle with D-47 fail-safe note, limit, "Send an alert", "Ring paired phones" with D-50) in web/src/components/settings/SafetyTab.tsx and web/src/components/settings/__tests__/
- [ ] T041 [US2] Vitest first, then the six battery events in a "Battery" group of web/src/components/settings/AlertsEventsCard.tsx (rows via `AlertEventRow`, D-48 disabled state), event keys/defaults in web/src/components/settings/alertsShared.ts and web/src/validation/alertsSchema.ts

## Phase 5: User Story 3 - Stretch the battery (P2)

**Goal**: power saving at critical, warning then deep sleep at shutdown, resume and power-restored alert.
**Independent test**: quickstart section 6.

### Tests

- [X] T042 [P] [US3] Native tests first in test/test_battery_logic/test_power_policy.cpp: Normal → Saving below critical (power saving on, not charging) with `enteredSaving`; Saving → Normal at critical + 5 or charging/USB with `powerRestored`; shutdown needs 2 consecutive valid reads ≤ shutdown; ShutdownPending → `sleepNow` after the grace and dispatcher idle, or at grace + 15 s regardless; USB during the grace cancels with `powerRestored`; missing/estimating data never advances towards sleep; power saving off → stays Normal; millis wrap
- [X] T043 [P] [US3] Native tests first in test/test_alert_logic/: going-to-sleep and power-restored one-shots are sent during the startup grace and ignore the cooldown; Wake level going-to-sleep adds bit 16 to `alarmFlags` (in test/test_device_core/)

### Implementation

- [X] T044 [US3] `PowerPolicy` in lib/BatteryLogic/include/BatteryLogic.h and lib/BatteryLogic/src/PowerPolicy.cpp; T042 passes
- [X] T045 [US3] One-shot battery inputs in lib/AlertLogic/src/AlertEngine.cpp and alarm flag in lib/DeviceCore/src/DeviceAlerts.cpp; T043 passes
- [X] T046 [US3] `AlertDispatcher::idle()` (queue empty and no delivery running) in include/AlertDispatcher.h and src/AlertDispatcher.cpp
- [X] T047 [US3] Power manager: apply the policy each gauge read — slow the sensor/snapshot interval to `saveReadIntervalSeconds`, set the spec 031 display `forcedOff`, feed `shutdownPending` to the safety inputs, wait out the grace, then `esp_sleep_enable_ext0_wakeup(pgPin, 0)` when set + a 60 min timer wake and `esp_deep_sleep_start()`; RTC flags `sleptForBattery`/wake cause, in include/PowerManager.h and src/PowerManager.cpp
- [X] T048 [US3] Early boot check in src/main.cpp right after `setupI2C()`: after a battery timer wake, read the gauge (and charger pins); still ≤ shutdown and no USB/charging → back to sleep silently; PG wake or charging → normal boot and a power-restored alert once WiFi is joined or after 2 min; `powerSaving` in status/readings from the policy
- [X] T049 [US3] Both firmware builds pass with `-Werror`; record sizes in specs/032-battery/budgets.md

## Phase 6: User Story 4 - Integrations know the battery (P2)

**Goal**: HA battery entities, `{battery*}` template variables, BLE summary fields.
**Independent test**: quickstart section 4.3-4.4.

### Tests

- [X] T050 [P] [US4] Native tests first in test/test_readings/: HA entities `battery`, `battery_voltage`, `battery_state` (enum options), `battery_time_to_empty` published with `Groups.battery`, removed with an empty payload otherwise; no battery entities for a gauge-less device
- [X] T051 [P] [US4] Native tests first in test/test_alert_variables/ and test/test_device_core/: `{battery}`, `{battery_voltage}`, `{battery_state}`, `{battery_time_left}` values and `--` without data; template-variables.json, `alertVars` and web alertVariables.ts stay in step; BLE summary contains `battery` and `batteryState` and fits 384 B (test/test_ble_logic or test_device_core, wherever the summary builder is tested)

### Implementation

- [X] T052 [US4] HA entities in lib/Readings/src/Readings.cpp; `Groups.battery` from presence and `mqtt.publish.battery` in src/WebServer.cpp (discovery republish on presence change); T050 passes
- [X] T053 [US4] Template variables in lib/AlertLogic/template-variables.json, lib/DeviceCore/src/DeviceAlerts.cpp and web/src/components/settings/alertVariables.ts; BLE summary `battery`/`batteryState` (`StaticJsonDocument<384>`) in src/WebServerAlerts.cpp; MQTT publish toggle "Battery" in the MQTT settings (web/src/components/settings/NetworkTab.tsx, D-49); T051 passes

## Phase 7: Settings UI (US1-US3)

- [ ] T054 Vitest first, then the Settings → Device → Battery card per contracts/ui.md (badge, monitoring toggle, thresholds, capacity, power saving with interval and grace, pins; disabled with D-46 except pins) in web/src/components/settings/BatteryCard.tsx, added to web/src/components/settings/DeviceTab.tsx after Display; Zod in web/src/validation/configSchema.ts, defaults in web/src/components/settings/defaults.ts, payload in web/src/components/settings/payload.ts; tests in web/src/components/settings/__tests__/

## Phase 8: Polish & cross-cutting

### Demo and contracts (FR-013)

- [ ] T055 [P] Contract schemas: `battery` in specs/016-demo-device-emulation/contracts/schemas/readings.schema.json and status.schema.json; config keys in config.schema.json; `batteryDetected` in settings-effective.schema.json; no existing property changed
- [ ] T056 Demo: simulated 7 Ah pack (discharging, "USB power" panel switch → charging → charged), presets "Battery low" and "Charge fault", battery in the emulated device, in web/src/demo/conditions.ts, web/src/demo/simulator.ts, web/src/demo/device.ts, web/src/demo/presets.ts, web/src/demo/panel/SensorInputs.tsx, tools/demo-core/sensor_feed.cpp and tools/demo-core/emulated_device*.cpp; rebuild the WASM core with tools/demo-core/build.sh (or note emsdk unavailable); `tools/contract-check.py` passes
- [ ] T057 Regenerate docs/api/openapi.json with `python tools/api/openapi.py` (schemas from the contract files, operations.json unchanged unless a new route appears); `--check` passes

### Strings (FR-018)

- [ ] T058 Every new UI string in web/src/i18n/en.json with a context note in web/src/i18n/en.context.json; translations written for all 13 locales in web/src/i18n/locales/*.json in each file's register; device-built messages (validation, safety reasons, alert titles, `battery-missing`) via `python3 tools/i18n/gen_device_catalog.py`; `python3 tools/i18n/translate.py --all --record`, `node tools/i18n/check.mjs`, `gen_device_catalog.py --check` pass (no paid API)

### Docs (FR-017, constitution VII)

- [ ] T059 [P] New docs/user-guide/battery.md: charging and power path, reading the gauge (estimating, accuracy), states, time estimates, alerts and their defaults, the safety rule and its options, power saving and deep sleep (wake on USB, hourly check without a PG pin), matched cells and polarity; nav entry in mkdocs.yml
- [ ] T060 [P] Hardware: the travel board (charger, gauge, protection, pins, power switch) in docs/hardware/overview.md and docs/getting-started/hardware.md
- [ ] T061 [P] docs/user-guide/configuration.md (battery keys, alpaca battery keys, mqtt.publish.battery, alert events), docs/reference/settings-dependencies.md (D-46..D-51, regenerated where generated)
- [ ] T062 [P] docs/api/rest.md and docs/api/websocket.md (`battery` in readings and status), docs/user-guide/mqtt.md (state `battery`, HA entities), docs/user-guide/ble.md (summary fields, alarm bits 14-16), docs/user-guide/alerts.md (six events, rule alert, variables), docs/user-guide/alpaca.md (battery rule, shutdown reason); Settings paths match en.json labels; `python3 -m mkdocs build --strict` where mkdocs is installed

### Budgets and gates (FR-016, constitution IV, VIII)

- [ ] T063 Measure after: firmware size both builds (SIZE-04; combined 031+032 growth recorded against the 95% line), static RAM, UI gzip delta ≤ 10 KB (SIZE-02), config/alerts parts/readings/status worst cases (tests); update specs/032-battery/budgets.md; device heap and loop `stackFree` on the spare device only, noted for the PR
- [ ] T064 Full gate: `pio test -e native`, both firmware builds, ConformU (T039), `tools/quality/check.py` (no new findings), `tools/api/openapi.py --check`, web `vitest run`, `tsc --noEmit`, `eslint src`, i18n, settings-deps and dashboard checks
- [ ] T065 Run quickstart.md sections 1 and 2 and record results; sections 3-6 on the spare device / a travel board with the owner's permission (never the main device), or recorded as open for the PR; changelog entry in CHANGELOG.md
- [ ] T066 Converge (`speckit-converge`) until it reports converged

## Dependencies

- Spec 031 first (T024 and T047 use its view and `forcedOff`; T008 extends its pin check).
- Phase 1 → Phase 2 → stories. T006 before T007/T008/T009; T013 before T020/T021; T019 before T021.
- US1 (Phase 3) is the MVP; US2 needs T021 (snapshot/readings) for its inputs; US3 needs T019, T035
  (shutdown reason) and T036 (alert types); US4 needs T020 and T037.
- T027 (mockup review) gates T028-T030, T040, T041, T054.
- Polish after the stories it documents; T056 after every `lib/*` change; T066 last.

## Parallel examples

- Phase 2: T004, T005, T010, T011 together.
- US1: T014-T018 together; T026 alongside T019-T023.
- US2: T032, T033, T034 together.
- US3: T042 and T043 together.
- US4: T050 and T051 together.
- Polish: T055 and T059-T062 together.

## Implementation strategy

1. MVP = Phases 1-3 (US1): battery visible everywhere on a board with a gauge, nothing changes elsewhere.
2. US2 (warnings, safety rule) completes the P1 scope: the device warns and imaging software can park.
3. US3 (power saving and sleep), then US4 (integrations), then the Settings card.
4. Polish: demo, schemas, translations, docs, budgets, convergence.
