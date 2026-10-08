---
description: "As-built task list (backfill) for Rain Sensor (Hydreon RG-15)"
---

# Tasks: Rain Sensor (Hydreon RG-15)

**Input**: Design documents from `specs/003-rain-sensor/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 RG-15 UART driver and polling in src/sensors/RG15Sensor.cpp

## Phase 2: Foundational

- [X] T002 rain config (pins, baud, resolution, units, intervals, daily reset) in src/Config.cpp

## Phase 3: User Story 1 - Know when it's raining (P1)

- [X] T003 [US1] Response parsing, isRaining and latched raining with rainClearDelayMs in src/sensors/RG15Sensor.cpp
- [X] T004 [US1] rainSensor payload (only when enabled) and /api/status rg15 entry in src/WebServer.cpp
- [X] T005 [US1] Rain rate to mm/h for Alpaca in lib/AlpacaLogic/src/ObservingConditionsMapper.cpp

## Phase 4: User Story 2 - See how much rain (P2)

- [X] T006 [US2] Daily total reset and local event accumulation in src/sensors/RG15Sensor.cpp
- [X] T007 [US2] Rain Sensor card with units and lens/emitter warnings in web/src/components/Dashboard.tsx

## Phase 5: User Story 3 - Bring up and service the sensor (P3)

- [X] T008 [US3] Communication test, reset-total and reboot routes with auth in src/WebServer.cpp
- [X] T009 [US3] Communication test in web/src/components/settings/SensorsTab.tsx; reset/reboot in web/src/components/System.tsx

## Phase 6: Polish & Cross-Cutting

- [X] T010 [P] RG-15 hardware guide in docs/hardware/rg15.md

## Phase 7: Convergence

- [X] T011 CRITICAL: Move the RG-15 response parsing and the rain latch (a safety input) into lib/ with native tests (latch hold/clear timing, garbled lines) per Constitution III (contradicts)
- [X] T012 Update docs/hardware/rg15.md: rainSensor is only present when the sensor is enabled (the `enabled` field no longer distinguishes off) per FR-006 / FR-009 (contradicts)
- [X] T013 Report each reading under one name — drop the aliases rain_intensity/rInt, total_accumulation/totalAcc, local_event_accumulation/event_accumulation, hydreon_event_accumulation/eventAcc in src/WebServer.cpp (appendRG15Diagnostics) and web/src/components/Dashboard.tsx per FR-008 (unrequested)
- [X] T014 Document every field the device sends (accumulation_since_last_read, rain_clear_delay_ms, timing and daily-reset fields) in docs/hardware/rg15.md per FR-008 / SC-003 (partial)
