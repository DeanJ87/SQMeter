---
description: "As-built task list (backfill) for Environment and Cloud Cover"
---

# Tasks: Environment and Cloud Cover

**Input**: Design documents from `specs/002-environment-cloud/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 BME280 and MLX90614 drivers on the shared I2C bus in src/sensors/BME280Sensor.cpp and src/sensors/MLX90614Sensor.cpp

## Phase 2: Foundational

- [X] T002 cloudDetection config with defaults and clear < overcast validation in src/Config.cpp

## Phase 3: User Story 1 - Know whether it's clear (P1)

- [X] T003 [US1] Humidity-corrected delta, linear cloud cover and condition in src/calculations/CloudDetection.cpp
- [X] T004 [US1] irTemperature and cloudConditions payload in src/WebServer.cpp
- [X] T005 [US1] Cloud Conditions and IR Temperature cards in web/src/components/Dashboard.tsx

## Phase 4: User Story 2 - See the local environment (P2)

- [X] T006 [US2] Magnus dew point in src/sensors/BME280Sensor.cpp
- [X] T007 [US2] environment payload and 53% humidity fallback flags in src/WebServer.cpp
- [X] T008 [US2] Environment card in web/src/components/Dashboard.tsx

## Phase 5: User Story 3 - Tune the cloud model (P3)

- [X] T009 [US3] Cloud detection card in web/src/components/settings/SensorsTab.tsx

## Phase 6: Polish & Cross-Cutting

- [X] T010 [P] Cloud detection section in docs/reference/sky-quality.md and cloudDetection fields in docs/user-guide/configuration.md

## Phase 7: Convergence

- [X] T011 CRITICAL: Move the cloud model (humidity correction, cloud %, condition) and the Magnus dew point into lib/ with native tests covering both threshold boundaries per Constitution III (contradicts)
- [ ] T012 Use the device's cloud condition for the dashboard label instead of fixed 15/40/70% bands in web/src/components/Dashboard.tsx (conditionLabel/conditionTone) per FR-007 / US1-AC3 (contradicts)
- [ ] T013 Show on the Cloud Conditions card when humidity is assumed (cloudConditions.humiditySource = "default") in web/src/components/Dashboard.tsx per FR-005 / SC-003 (missing)
- [ ] T014 Display temperatures as °C (not "C") on the Environment and Cloud Conditions tiles in web/src/components/Dashboard.tsx per FR-007 / Constitution V (contradicts)
- [X] T015 Compute cloud metrics once per reading and reuse them for the sensor payload, Alpaca snapshot and safety inputs in src/WebServer.cpp (three CloudDetection call sites) per FR-008 (partial)
- [X] T016 Document the humidity correction, thresholds and 53% fallback in the cloud section of docs/reference/sky-quality.md per FR-009 (partial)
