# Tasks: Rain needs cloud

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/interfaces.md](contracts/interfaces.md)

**Tests**: native tests are required by Principle III and the spec's success criteria (SC-001..003).

## Phase 1: Setup

- [X] T001 Amend Principle I (MINOR, 1.3.1 → 1.4.0) with the opt-in corroboration rule and its fail-safe in .specify/memory/constitution.md

## Phase 2: Foundational (blocks every story)

- [X] T002 Config keys `alpaca.rainNeedsCloud` (bool, default false) and `alpaca.rainNeedsCloudPercent` (whole number, default 10, range 1-100) in lib/ConfigModel/include/Config.h, src/ConfigModel.cpp, ConfigJsonIn.cpp, ConfigJsonOut.cpp, ConfigValidate.cpp; range in test/fixtures/config-ranges.json
- [X] T003 Thresholds `rainNeedsCloud`/`rainNeedsCloudPercent` from the config in lib/AlpacaLogic/include/SafetyEvaluator.h and lib/DeviceCore/src/DeviceSafety.cpp
- [X] T004 [P] Native config tests: defaults, older config without the keys loads off, range errors, round trip in test/test_config_model/test_main.cpp

## Phase 3: US1 + US2 + US3 - the verdict (P1)

**Goal**: dew under a healthy clear sky is safe and flagged; cloud-confirmed rain and rain with an
untrusted sky are unsafe; trusted rain is latched.
**Independent test**: `pio test -e native -f test_rain_trust -f test_alpaca_safety -f test_device_core`.

- [X] T005 [US1] Native tests for the classification and latch, including the 20 Nov 19:43 dew replay and the 22 Nov 05:49 shower replay, in test/test_rain_trust/test_main.cpp
- [X] T006 [US1] `RainClass`, `RainEvidence`, `classifyRain()` and `RainTrustLatch` in lib/AlpacaLogic/include/RainTrust.h and lib/AlpacaLogic/src/RainTrust.cpp
- [X] T007 [US1] `SafetyInputs` rain evidence (`skyCloudUsable`, `rainRateMmPerHour`, `rainClass`) and `SafetyResult` dew fields; `checkRain` wording per research D4 in lib/AlpacaLogic/src/SafetyEvaluator.cpp; tests in test/test_alpaca_safety
- [X] T008 [US3] Sky usability (IR sensor initialised, OK, fresh, cloud computable) and rain rate in `Core::safetyInputs`, lib/DeviceCore/src/DeviceCore.cpp; tests in test/test_device_core
- [X] T009 [US1] Firmware owns a `RainTrustLatch`, updated once per safety pass, in include/WebServer.h and src/WebServerAlerts.cpp
- [X] T010 [US1] The demo owns one too in tools/demo-core/emulated_device.{h,cpp}; rebuild the core (tools/demo-core/build.sh)

## Phase 4: US5 - dew suspected is visible and can alert (P3, needed by US1's reporting)

- [X] T011 [US5] `SafetyStatus` dew fields and `dewSinceMs` in lib/DeviceCore/include/SafetyStatus.h; `Core::updateSafety` and `Core::writeSafety` write `dewSuspected` and `dew` in lib/DeviceCore/src/DeviceCore.cpp; `SAFETY_CAPACITY` checked
- [X] T012 [US5] HA binary sensor `dew_suspected` (device class problem, `<base>/safety`) in lib/Readings/src/Readings.cpp; tests in test/test_readings
- [X] T013 [US5] Alert: `AlertType::DewSuspected`, `dew_suspected` event (default off), `AlertRules.onDewSuspected`, `AlertInputs.dewSuspected` and wording in lib/AlertLogic, lib/DeviceCore/src/DeviceAlerts.cpp, lib/ConfigModel (event list); tests in test/test_alert_logic
- [X] T014 [US5] Dashboard rain card note "Rain under a clear sky: dew suspected" from `safety.dewSuspected` in web/src/components/dashboard/weatherCards.tsx and cards.tsx; `SafetyStatus` type in web/src/types/readings.ts; inventory entry in web/src/dashboard/inventory.json; Vitest test
- [X] T015 [US5] Alerts settings row for `dew_suspected` in web/src/components/settings/AlertsEventsCard.tsx, alertsShared.ts, types/alerts.ts, defaults.ts, validation, mocks

## Phase 5: US4 - the owner can turn it on and understand it (P2)

- [X] T016 [US4] Settings dependency D-52 (`alpaca.rainNeedsCloud`: rain sensor on, IR sky sensor detected; unmet inactive) and `dew_suspected` in D-04/D-05 in lib/SettingsDeps/catalogue.json, lib/SettingsDeps/src/SettingsDeps.cpp, web/src/lib/settingsDeps.ts, fixtures and docs/reference/settings-dependencies.md
- [X] T017 [US4] Rule row "Rain needs cloud" with the level field "Cloud to confirm rain" (%, 1-100) and the "?" text in web/src/components/settings/SafetyTab.tsx; types, defaults, depLabels, mocks, Zod schema; Vitest test
- [X] T018 [US4] English strings and context notes in web/src/i18n/en.json and en.context.json; device message catalogue regenerated; every locale in web/src/i18n/locales translated

## Phase 6: Polish

- [X] T019 [P] Docs: user guide safety/alerts/dashboard/MQTT, REST reference, settings reference, OpenAPI (tools/api + regenerate), CHANGELOG
- [X] T020 [P] Contract schemas (specs/016-demo-device-emulation/contracts/schemas) for config, safety and readings
- [X] T021 Quality gates: native tests, both firmware builds, web typecheck/vitest/lint, i18n checks, check.py, copy/label checks, size baselines, mkdocs --strict, openapi --check

## Dependencies

Phase 2 before everything. T006 before T007-T010. T011 before T012-T014. T016 before T017.
US4's UI (Phase 5) only needs Phase 2 and T016.

## Parallel opportunities

T004 with T005; T012, T013 and T014 after T011; T019 and T020 at any point after the interfaces settle.

## Implementation strategy

MVP is Phase 2 + Phase 3 (the verdict, the owner enables it over `/api/config`); then reporting
(Phase 4), the settings UI (Phase 5) and docs.
