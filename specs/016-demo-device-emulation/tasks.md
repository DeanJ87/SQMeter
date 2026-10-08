# Tasks: Demo That Behaves Like the Device

**Input**: Design documents from `/specs/016-demo-device-emulation/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Requested by the spec (FR-010, SC-001/002/004/005) and required by Constitution III for code
moved into `lib/`.

## Phase 1: Setup

- [X] T001 Add Emscripten (pinned) instructions and version file in tools/demo-core/VERSION and a build script tools/demo-core/build.sh that compiles lib/*/src/*.cpp + tools/demo-core/bridge.cpp with embind to web/src/demo/core/sqm-core.mjs (+ .wasm), `-O2 -sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH`
- [ ] T002 [P] Add `npm run build:core` (calls tools/demo-core/build.sh) and `ajv` dev dependency in web/package.json
- [X] T003 [P] Add native test env include paths for the new libs (lib/SensorTypes, lib/ConfigModel, lib/DeviceCore) in platformio.ini

## Phase 2: Foundational (move device logic into lib/, no behaviour change)

**Purpose**: One implementation for firmware, native tests and the demo (research R1/R2). Blocks all stories.

- [X] T004 Move the plain reading structs (TSL2591Reading, TSL2591Diagnostics, BME280Reading, MLX90614Reading, GPSReading, RG15Reading, RG15Diagnostics, RG15State, WindReading, SensorStatus) into lib/SensorTypes/include/SensorTypes.h; sensor class headers in include/sensors/ include it
- [X] T005 Move the Config struct and its model code (createDefault, toJson, alertsToJson, fromJson, applyJson, validate) from include/Config.h + src/Config.cpp into lib/ConfigModel/{include/Config.h,src/ConfigModel.cpp} without Logger/Preferences; keep NVS load/save in src/ConfigStore.cpp; firmware behaviour identical
- [X] T006 [P] Native tests for ConfigModel (defaults round-trip, secret redaction + placeholder preservation, every validate() rule's boundary, fromJson merge onto a base) in test/test_config_model/test_main.cpp
- [X] T007 Create lib/DeviceCore with `SensorSnapshot` (moved from include/WebServer.h) and the derivations from refreshSensorSnapshot (sky, cloud, humidityMeasured/cloudHumidity) in lib/DeviceCore/{include/DeviceCore.h,src/DeviceCore.cpp}
- [X] T008 Move buildReadings, appendDiagnostics/appendLightDiagnostics/appendRainDiagnostics, readingStatus and cloudConditionName from src/WebServer.cpp into lib/DeviceCore (pure: snapshot + config + now in, Readings::Snapshot / JSON out)
- [X] T009 Move buildAlpacaSafetyInputs, buildAlpacaSafetyThresholds, buildAlpacaObservingConditionsSnapshot, computeNight and the safety-status document (appendSafetyStatus) into lib/DeviceCore
- [X] T010 Move alertVars, applyAlertTemplate, eventSettingFor and the alert decision part of processAlerts (inputs/rules from config, engine update, level/sound/template, stacking, BLE alarm flags) into lib/DeviceCore as `AlertStep run(...)` returning what to dispatch; WebServer keeps dispatch, BLE and NVS
- [X] T011 Move the status document's decision parts from createStatusJson into lib/DeviceCore (`writeSky`, `writeSensorHealth`, `writeDiagnostics`); the hardware sections (firmware/heap/partitions/wifi/mqtt/ble/time/OTA) stay in the firmware and are emulated by the demo, guarded by the status schema (T030-T032)
- [X] T012 src/WebServer.cpp calls lib/DeviceCore for everything moved in T007-T011; no change in REST/WS/MQTT/Alpaca output (compare `/api/sensors`, `/api/status`, `/api/safety` from a device before/after)
- [X] T013 [P] Native tests for DeviceCore (readings statuses incl. missing/no age, safety reasons + safe delay, night computation, alert templating/vars, status document keys) in test/test_device_core/test_main.cpp
- [X] T014 Verify: `pio test -e native`, both firmware builds (state flash delta), ConformU against tools/alpaca-sim, OTA to the spare and compare responses

**Checkpoint**: firmware unchanged in behaviour; all device decisions available as portable C++.

## Phase 3: User Story 1 - Everything in the demo works (P1) 🎯 MVP

**Goal**: One emulated device answers every page and link; no contradictions.

**Independent Test**: crawl the demo; follow every Alpaca link; dashboard, Alpaca page and Alpaca live state agree.

- [X] T015 [US1] Write tools/demo-core/bridge.cpp: embind class EmulatedDevice per contracts/device-core.md (constructor, getConfig, applyConfig, restart, tick, readings, status, safety, alerts, clearAlerts, setArmed, armed, testAlert, alpaca, history, mqttMessages, discovery), using DeviceCore + Alpaca::Router with a DeviceCore-backed Backend; no I/O, time only from tick
- [X] T016 [US1] Build the core (T001) and commit web/src/demo/core/sqm-core.mjs + sqm-core.wasm
- [X] T017 [US1] web/src/demo/device.ts: load the core, create the device, drive tick() every second from the browser clock and the simulator, expose typed wrappers
- [X] T018 [P] [US1] web/src/demo/simulator.ts: baseline sky inputs (lux from sun altitude via the core's sun position, IR temps, BME, wind) per data-model.md "Simulator inputs"; sensors absent when disabled in config
- [X] T019 [US1] web/src/demo/handlers.ts: MSW handlers for every endpoint in web/src/mocks/handlers.ts routed to the device (REST, /ws/sensors, /ws/status, /management/*, /api/v1/*); demo build (web/src/main.tsx) uses these instead of src/mocks/handlers.ts
- [X] T020 [US1] Direct URLs (contracts/demo-urls.md): copy index.html to 404.html in the demo build (web/vite.demo.config.ts); web/src/demo/ApiView.tsx renders `/api/...` and `/management/...` from device.alpaca()/REST with status; `/setup...` redirects to `#/settings?tab=safety`
- [X] T021 [P] [US1] Playwright web/tests/demo.spec.ts "demo links": visit every page and every in-app link incl. Alpaca links and direct API URLs; assert no host 404 page
- [X] T022 [P] [US1] Playwright web/tests/demo.spec.ts "rain: ... agree": safety on dashboard, Alpaca page and Alpaca live state agree (baseline and after rain)

## Phase 4: User Story 2 - Settings change the emulated device (P1)

**Goal**: Saved settings take effect as on a device.

**Independent Test**: quickstart scenario 3.

- [X] T023 [US2] POST/PUT /api/config → device.applyConfig (device's own validation/messages); GET /api/config → device.getConfig(true); restart-required changes queued until POST /api/restart (simulated reboot: new boot in history, uptime reset)
- [X] T024 [US2] Persistence per data-model.md "Persisted demo state": `sqm.demo.v1` in sessionStorage `{ version: 1, config, armed, safetyHistory, alerts, scenario, timeMultiplier, savedAt }`; corrupt/unknown version → defaults; storage errors → in-memory
- [X] T025 [P] [US2] Playwright web/tests/demo.spec.ts "settings change the emulated device": GPS off removes gps group; location drives sun altitude; tighter cloud limit → unsafe reason; rain off → no rain group and no rain reason

## Phase 5: User Story 3 - The demo is safe to publish (P1)

**Goal**: Nothing leaves the browser.

**Independent Test**: quickstart scenario 4.

- [X] T026 [US3] Simulated outbound actions in web/src/demo/handlers.ts: update check (canned release list shaped like the device's), update apply (progress over /ws/status, then simulated restart), uploads (accept, don't keep the file), alert channel tests and event tests (device.testAlert; result "demo"), MQTT test, WiFi scan/connect, restart; each response carries `demo: true`
- [X] T027 [US3] Show "Demo: nothing was sent" where a `demo: true` result is displayed (AlertsTab, NetworkTab MQTT test, Updates, WifiSetup) using existing Note/ResultNote components
- [X] T028 [US3] Content-Security-Policy `connect-src 'self'` (meta tag in the demo build only) in web/vite.demo.config.ts
- [X] T029 [P] [US3] Playwright web/tests/demo.spec.ts "nothing leaves the browser": exercise every action; assert every request is same-origin and for a file under the demo

## Phase 6: User Story 4 - The demo can't drift (P2)

**Goal**: Contract mismatches fail the build.

**Independent Test**: change a field in a schema; tests fail.

- [ ] T030 [P] [US4] JSON Schemas in specs/016-demo-device-emulation/contracts/schemas/{readings,status,safety,config,alpaca-management}.schema.json derived from specs/013 contracts and the device
- [ ] T031 [US4] Vitest web/src/demo/__tests__/contracts.test.ts validates every emulated document (ajv) against the schemas
- [ ] T032 [P] [US4] tools/contract-check.py validates a real device's /api/sensors, /api/status, /api/safety, /api/config, /management/v1/* against the same schemas
- [ ] T033 [US4] CI: rebuild the core with the pinned Emscripten and fail if web/src/demo/core differs (.github/workflows/docs.yml or build.yml)

## Phase 7: User Story 5 - Make things happen (P2)

**Goal**: Weather and fault scenarios drive the device.

**Independent Test**: quickstart scenario 6.

- [X] T034 [US5] Scenarios in web/src/demo/simulator.ts: 'rain' | 'cloud' | 'clear' | 'fail-light' | 'fail-ir' | 'fail-environment' | 'fail-rain' | 'dawn', one active at a time, ending back at baseline; demo time multiplier (1× or 10×) shortens rain clear delay, safe delay and cooldowns
- [X] T035 [US5] web/src/demo/DemoPanel.tsx: floating panel (shared ui.tsx components, phone width) with scenario buttons, time multiplier, "Reset demo", and a one-line "This is a simulated SQMeter" note; mounted only in the demo build
- [X] T036 [P] [US5] Playwright web/tests/demo.spec.ts "rain" scenario: rain → unsafe "Rain detected" + rain alert at its level; clears after the (shortened) rain clear delay; fail-light → sensor fault reason

## Phase 8: User Story 6 - Own domain (P3)

- [X] T037 [US6] Done in #78 (sqmeter.dev / demo.sqmeter.dev, redirects); verify the demo still works at a sub-path with DEMO_BASE (build + preview)

## Phase 9: Polish

- [ ] T038 [P] Rewrite docs/live-demo.md: what's real (device logic) vs simulated (sensors, outbound), scenarios, Reset, persistence
- [ ] T039 [P] Contributing: device core build (docs/development/contributing.md)
- [ ] T040 Regenerate screenshots from the emulated device (web/tests/screenshots.spec.ts)
- [ ] T041 Run quickstart.md end to end; update specs/README.md status for 016

## Dependencies

- Phase 2 blocks everything (the core needs the moved logic).
- US1 (Phase 3) builds the core and the handlers that US2-US5 extend.
- US2, US3, US4 can proceed in parallel after US1; US5 after US1 (simulator).

## Implementation Strategy

MVP = Phase 2 + US1 (one consistent emulated device, working links). Then US2 + US3 (settings, safety),
then US4/US5, then polish. Phase 2 ships to the firmware first, verified on a device.
