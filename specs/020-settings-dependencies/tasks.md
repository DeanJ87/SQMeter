---

description: "Tasks for spec 020: settings dependencies"
---

# Tasks: Settings Dependencies

**Input**: `specs/020-settings-dependencies/` - spec.md, plan.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: required by the spec (FR-013, FR-014, SC-001..SC-004).

**Organization**: by user story; Phase 2 is the shared evaluator every story uses.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

- [X] T001 Write the machine-readable catalogue (36 entries, reasons with text and fix target) in lib/SettingsDeps/catalogue.json
- [X] T002 [P] Add `-Ilib/SettingsDeps/include` to the native env in platformio.ini and `lib/SettingsDeps/src/*.cpp` to tools/demo-core/build.sh

## Phase 2: Foundational (blocks every story)

- [X] T003 Implement `Deps::evaluate`, `find`, `reasonFor`, `rulesNotInEffect`, `writeReport`, `reportCapacity` in lib/SettingsDeps/include/SettingsDeps.h and lib/SettingsDeps/src/SettingsDeps.cpp (chains in catalogue order; first unmet link decides; safety rules carry `unmet`; D-14/D-25 `neutral`)
- [X] T004 Add `Core::sensorFacts` (detection = status ok or stale; GPS running/fix) to lib/DeviceCore/include/DeviceCore.h and lib/DeviceCore/src/DeviceCore.cpp
- [X] T005 [P] Write shared parity fixtures test/fixtures/settings-deps/cases.json (met and unmet case for every reported ID) and generate test/fixtures/settings-deps/default-config.json from `Config::createDefault()`
- [X] T006 Native tests in test/test_settings_deps/test_main.cpp: fixtures, reasons/settings match catalogue.json, default-config fixture current, report document size and shape
- [X] T007 [P] TypeScript mirror `evaluate`, `viewOf`, `effectiveEntries`, `newlyInactive`, `blocksSwitchingOn`, `REASONS`, `FIX_LABEL` in web/src/lib/settingsDeps.ts (unknown facts never make a setting inactive; `blockedBy` for off settings)
- [X] T008 [P] Vitest parity in web/src/lib/__tests__/settingsDeps.test.ts on the same fixtures and catalogue

## Phase 3: User Story 1 - Nothing claims to be on when it can't work (P1)

**Independent test**: switch a dependency off; the dependent shows "Inactive - <reason>" with a fix, the report marks it inactive, the device doesn't act on it.

- [X] T009 [US1] `GET /api/settings/effective` and `settingsFacts()` in src/WebServer.cpp, include/WebServer.h
- [X] T010 [US1] Inactive alert channels recorded `skipped` with the reason, never attempted: `ChannelBlocks` in include/AlertDispatcher.h, src/AlertDispatcher.cpp; `channelBlocks()` used by every `dispatch` call in src/WebServer.cpp (tests ignore "alerts are off")
- [X] T011 [US1] Alerts follow N.I.N.A. only while Alpaca is on (`dep: D-12`) in src/WebServer.cpp and tools/demo-core/bridge.cpp
- [X] T012 [US1] Home Assistant Alerts switch only while alerts can go out: `Groups::alertsSwitch` in lib/Readings/include/Readings.h, lib/Readings/src/Readings.cpp, set in src/WebServer.cpp (`dep: D-13`); native test in test/test_readings/test_main.cpp
- [X] T013 [US1] Safety document `rulesNotInEffect` in lib/DeviceCore/src/DeviceCore.cpp; native test in test/test_device_core/test_main.cpp
- [X] T014 [US1] `DepNote` and `DepToggle` (blocked when off, always switchable off, warn vs neutral tone, one-click fix) in web/src/components/settings/controls.tsx; `deps` and `fix` props in web/src/components/settings/context.ts
- [X] T015 [US1] Settings loads the report on load, after save and with the status refresh, builds the view and the fix action (tab#anchor or restart) in web/src/components/Settings.tsx
- [X] T016 [P] [US1] Alerts tab: channels, events, Wake-me phones note, night-only options, arming via the view; counts only active channels; "Alerts reach nowhere" warning; no Send test on inactive channels - web/src/components/settings/AlertsTab.tsx
- [X] T017 [P] [US1] Safety tab rules with "Not in effect" (D-15) vs "Reports unsafe" (D-16..D-19) - web/src/components/settings/SafetyTab.tsx
- [X] T018 [P] [US1] Network (publish groups neutral, discovery, alerts-switch note, mDNS), Time (Sun & Moon, NTP, GPS restart/fix), Sensors (calibration, daily reset, vane), Device (OTA, Bluetooth, phone alarm) in web/src/components/settings/{NetworkTab,TimeTab,SensorsTab,DeviceTab}.tsx; section ids `security`, `time-sources`, `sky-sensors`
- [X] T019 [US1] Demo: `effective()` and skipped-with-reason channels in tools/demo-core/bridge.cpp; endpoint in web/src/demo/device.ts and web/src/demo/handlers.ts; dev mock in web/src/mocks/handlers.ts
- [X] T020 [US1] UI test per catalogue ID (note, fix action, locked/unlocked) in web/src/components/__tests__/SettingsDependencies.test.tsx with web/src/test/mockDevice.ts

## Phase 4: User Story 2 - Switching something off never loses my setup (P1)

- [X] T021 [US2] Round trip off → save → restart → on → save, values identical and active, for MQTT, rain, wind, Alpaca, alerts in test/test_settings_deps/test_main.cpp
- [X] T022 [US2] UI keeps an inactive dependent's value when saving in web/src/components/__tests__/SettingsDependencies.test.tsx; demo round trip in web/tests/settings-deps.spec.ts

## Phase 5: User Story 3 - The same answer everywhere (P2)

- [X] T023 [US3] Device report wins for settings that are on while the form is clean; local preview otherwise (`effectiveEntries`) - web/src/lib/settingsDeps.ts
- [X] T024 [US3] "Saving makes these inactive: ..." preview above the save bar in web/src/components/Settings.tsx; labels in web/src/components/settings/depLabels.ts
- [X] T025 [US3] Demo agreement (API-only setup → report, Settings page and delivery agree) in web/tests/settings-deps.spec.ts
- [X] T026 [P] [US3] Contract: settings-effective schema, `rulesNotInEffect` in safety/readings schemas (specs/016-demo-device-emulation/contracts/schemas), `tools/contract-check.py`, `tools/contracts/generate_schemas.py`, web/src/demo/__tests__/contracts.test.ts

## Phase 6: User Story 4 - New settings can't forget their dependencies (P2)

- [X] T027 [US4] Checker tools/settings-deps/check.py (implementation in both evaluators, native + web test references, fixture coverage, constraint messages, `dep: D-NN` markers and the undeclared-dependency scan, generated docs)
- [X] T028 [US4] Checker tests tools/settings-deps/test_check.py (dummy undeclared dependency fails and is named; marker passes; entry without tests fails)
- [X] T029 [US4] Markers for feature entries (D-20 AlpacaRouter, D-21/D-22 ObservingConditionsMapper) and tags in existing native tests (test_alpaca_logic, test_config_model)
- [X] T030 [US4] Run the checker and its tests in CI (.github/workflows/build.yml, path filters for test/** and tools/settings-deps/**)

## Phase 7: User Story 5 - Invalid combinations are still refused, clearly (P3)

- [X] T031 [US5] Web UI constraint messages = the device's (D-27, D-33, D-34); drop the UI-only auth-username rule; D-32 no longer rejected - web/src/validation/configSchema.ts, web/src/__tests__/configSchema.test.ts
- [X] T032 [P] [US5] Native constraint test with messages in test/test_config_model/test_main.cpp

## Phase 8: Polish

- [X] T033 [P] Docs: docs/api/rest.md (`/api/settings/effective`, `rulesNotInEffect`, skipped channels), docs/user-guide/configuration.md, docs/user-guide/alerts.md, generated docs/reference/settings-dependencies.md, mkdocs.yml nav
- [X] T034 Rebuild the demo core (tools/demo-core/build.sh) and SOURCE_HASH
- [X] T035 Verify: `pio test -e native`, both firmware builds (flash +10.1 KB / +10.2 KB), web typecheck + Vitest, demo build + Playwright, mkdocs --strict, checker
- [ ] T036 On-device checks on the spare device (listed in quickstart.md and the PR): contract-check, MQTT skipped delivery, Home Assistant switch removal, heap/stack after opening Settings

## Dependencies

- Phase 2 blocks everything. US1 before US2/US3 (they test what US1 builds). US4 needs US1's markers. US5 is independent.

## Phase 9: Convergence

- [X] T037 Show "Phones won't ring" neutrally (D-08 `neutral`) so a fresh standard-build device - whose default rain and sensor-fault events are at Wake me - shows no inactive warning, in lib/SettingsDeps/catalogue.json, lib/SettingsDeps/src/SettingsDeps.cpp, web/src/lib/settingsDeps.ts and test/fixtures/settings-deps/cases.json per SC-005 (partial)
- [X] T038 Don't offer "Test" on an inactive event row (reason shown instead) in web/src/components/settings/AlertsTab.tsx, with a UI test, per spec Edge Cases "Test sends" (partial)
- [X] T039 Add the missing "can't be switched on" (locked) UI cases for D-12, D-14, D-15, D-16, D-17, D-19 and D-25 in web/src/components/__tests__/SettingsDependencies.test.tsx per FR-013 (partial)
- [X] T040 Use the evaluator's clock threshold (2024-01-01, `Core::CLOCK_VALID_EPOCH`) for the RG-15 daily reset in src/sensors/RG15Sensor.cpp, so "the device doesn't know the time yet" (D-24) and the device agree, per FR-002 (contradicts)

## Phase 10: Convergence

- [X] T041 Show the rain rules' "Not in effect - Rain sensor is off" neutrally (D-15 `neutral`): both rules ship on while the rain sensor ships off, so a fresh device warned twice, in lib/SettingsDeps/catalogue.json, lib/SettingsDeps/src/SettingsDeps.cpp, web/src/lib/settingsDeps.ts and test/fixtures/settings-deps/cases.json, with an automated fresh-defaults test in web/src/lib/__tests__/settingsDeps.test.ts per SC-005 (partial)
