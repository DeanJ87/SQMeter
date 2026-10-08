---
description: "As-built task list (backfill) for Wind"
---

# Tasks: Wind (Anemometer and Vane)

**Input**: Design documents from `specs/004-wind/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 WindAggregator with 600 s history in lib/WindLogic/src/WindAggregator.cpp and tests in test/test_wind_logic/test_main.cpp

## Phase 2: Foundational

- [X] T002 wind config and pin validation in src/Config.cpp; live reconfigure in src/main.cpp

## Phase 3: User Story 1 - Wind speed and gusts (P1)

- [X] T003 [US1] Debounced pulse ISR in src/sensors/WindSensor.cpp
- [X] T004 [US1] Model presets and custom factor in web/src/components/settings/SensorsTab.tsx

## Phase 4: User Story 2 - Wind direction (P2)

- [X] T005 [US2] Vane ADC decoding, north offset, fault detection in src/sensors/WindSensor.cpp

## Phase 5: User Story 3 - Wind safety limits (P2)

- [X] T006 [US3] Wind limits in web/src/components/settings/SafetyTab.tsx

## Phase 6: Polish & Cross-Cutting

- [X] T007 [P] Wind card in web/src/components/Dashboard.tsx; guide in docs/hardware/wind.md

## Phase 7: Convergence

- [ ] T008 Update settings paths in docs/hardware/wind.md: "Settings → Wind (anemometer)" is now Settings → Sensors → Wind, and "Settings → ASCOM Alpaca & Safety → Wind" is Settings → Safety → Safety rules → Wind, per FR-006 / SC-002 (contradicts)
- [ ] T009 Use the UI's label "Model" (not "Anemometer type") in docs/hardware/wind.md per FR-006 (contradicts)
- [ ] T010 Highlight wind gust (and speed) against the configured safety limits instead of a fixed 10 m/s in web/src/components/Dashboard.tsx per FR-005 (contradicts)
