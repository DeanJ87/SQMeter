---
description: "As-built task list (backfill) for Sky Quality Measurement"
---

# Tasks: Sky Quality Measurement

**Input**: Design documents from `specs/001-sky-quality/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill. These tasks record work already present in `main` @ `b1d382e`; they are
marked done. `/speckit-converge` appends any remaining gaps as a Convergence phase.

## Phase 1: Setup

- [X] T001 TSL2591 driver integration and sensor base class in src/sensors/TSL2591Sensor.cpp

## Phase 2: Foundational

- [X] T002 Fixed-size raw-count sample buffers and rolling average in src/sensors/TSL2591Sensor.cpp
- [X] T003 [P] skyAveraging and skyCalibration config (defaults, validation, JSON) in src/Config.cpp

## Phase 3: User Story 1 - See how dark the sky is (P1)

- [X] T004 [US1] Night mode (MAX gain, 600 ms) and lux→SQM conversion in src/sensors/TSL2591Sensor.cpp and src/calculations/SkyQuality.cpp
- [X] T005 [US1] lightSensor and skyQuality objects in /api/sensors and /ws/sensors in src/WebServer.cpp
- [X] T006 [US1] Sky Quality hero card with live/stale state and not-detected card in web/src/components/Dashboard.tsx

## Phase 4: User Story 2 - Understand the reading (P2)

- [X] T007 [US2] NELM (Unihedron) and Bortle table with descriptions in src/calculations/SkyQuality.cpp
- [X] T008 [US2] Bortle, NELM and illuminance tiles in web/src/components/Dashboard.tsx

## Phase 5: User Story 3 - Calibrate against a reference (P2)

- [X] T009 [US3] Dark offset capture via POST /api/sensors/tsl2591/calibrate-dark in src/WebServer.cpp
- [X] T010 [US3] SQM offset applied when calibration is enabled; raw and calibrated SQM reported in src/sensors/TSL2591Sensor.cpp

## Phase 6: User Story 4 - Daylight and twilight (P3)

- [X] T011 [US4] Saturation clamp, flag and auto-ranging in src/sensors/TSL2591Sensor.cpp

## Phase 7: Polish & Cross-Cutting

- [X] T012 [P] Sky-quality reference and ADR-001 in docs/reference/sky-quality.md and docs/development/adr-001-sky-optics-and-averaging.md
- [X] T013 [P] skyAveraging/skyCalibration fields in docs/user-guide/configuration.md

## Phase 8: Convergence

- [ ] T014 CRITICAL: Move the lux→SQM, NELM and Bortle conversion from src/calculations/SkyQuality.cpp into lib/ and add native tests covering every Bortle boundary and NELM below 15 per Constitution III (contradicts)
- [ ] T015 Seed the dashboard SQM trend with real readings only — remove the synthetic sine history in web/src/components/Dashboard.tsx per FR-009 (contradicts)
- [ ] T016 Add dark calibration, SQM offset and averaging-window controls to web/src/components/settings/SensorsTab.tsx per FR-010 / US3-AC3 (missing)
- [ ] T017 Make docs/reference/sky-quality.md use the firmware's SQM constant (12.6, not 12.59) per FR-001 / FR-012 (contradicts)
- [ ] T018 Align the Bortle class 1 boundary between docs/reference/sky-quality.md ("> 21.99") and src/calculations/SkyQuality.cpp (">= 21.99") per FR-007 (contradicts)
- [ ] T019 Describe NELM in docs/reference/sky-quality.md as the Unihedron formula with NELM = 0 below SQM 15, not "based on atmospheric conditions" per FR-006 / FR-012 (partial)
- [ ] T020 Reject or warn on dark calibration before the averaging window has filled in src/WebServer.cpp (handleTSL2591DarkCalibration) per US3-AC1 (partial)
