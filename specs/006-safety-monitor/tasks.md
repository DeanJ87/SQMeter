---
description: "As-built task list (backfill) for Safety Monitor"
---

# Tasks: Safety Monitor

**Input**: Design documents from `specs/006-safety-monitor/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 SafetyEvaluator and SafeDelayFilter in lib/AlpacaLogic/src/SafetyEvaluator.cpp with tests

## Phase 2: Foundational

- [X] T002 alpaca thresholds, defaults and validation in src/Config.cpp

## Phase 3: User Story 1 - A verdict I can trust (P1)

- [X] T003 [US1] Rain/wind independent of freshness; fresh-data gating; unmeasurable limits unsafe in lib/AlpacaLogic/src/SafetyEvaluator.cpp
- [X] T004 [US1] Reasons with measured value and limit in lib/AlpacaLogic/src/SafetyEvaluator.cpp
- [X] T005 [US1] Inputs from the sensor snapshot and 1 s evaluation in src/WebServer.cpp

## Phase 4: User Story 2 - Safe delay (P2)

- [X] T006 [US2] Safe delay from boot and "Safe in Ns" in src/WebServer.cpp and web/src/components/SafetyCard.tsx

## Phase 5: User Story 3 - See why (P2)

- [X] T007 [US3] RTC safety history and /api/safety/history in src/SafetyHistory.cpp and src/WebServer.cpp
- [X] T008 [US3] Safety card with reasons and History in web/src/components/SafetyCard.tsx

## Phase 6: User Story 4 - Use the verdict anywhere (P3)

- [X] T009 [US4] /api/safety, /api/safe, ws safety and MQTT safe/safety in src/WebServer.cpp

## Phase 7: Polish & Cross-Cutting

- [X] T010 [P] Safety rules in web/src/components/settings/SafetyTab.tsx and docs/user-guide/alpaca.md

## Phase 8: Convergence

- [X] T011 CRITICAL: Move the safety-history ring buffer and last-alert lookup (they decide what the alert engine is seeded with after a restart) into lib/ with native tests per Constitution III (partial)
- [X] T012 Update docs/user-guide/alpaca.md Safety rules: "Settings → ASCOM Alpaca & Safety" is now Settings → Safety, and the "Force SafetyMonitor unsafe" checkbox is the "Force unsafe" toggle, per FR-008 / SC-003 (contradicts)
- [X] T013 State in docs/user-guide/alpaca.md that reasons carry the measured value and limit, and link the dashboard History, per FR-004 / FR-007 (partial)
