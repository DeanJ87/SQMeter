---
description: "As-built task list (backfill) for Alerts"
---

# Tasks: Alerts

**Input**: Design documents from `specs/008-alerts/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 AlertEngine with edge triggering, cooldown and settle times in lib/AlertLogic/src/AlertEngine.cpp with tests

## Phase 2: Foundational

- [X] T002 alerts config, per-event settings, migration and validation in src/Config.cpp
- [X] T003 AlertDispatcher channels, TLS, retries and delivery records in src/AlertDispatcher.cpp

## Phase 3: User Story 1 - Right volume (P1)

- [X] T004 [US1] Levels, sounds, stacking and BLE wake in src/WebServer.cpp and lib/AlertLogic/src/AlertEngine.cpp
- [X] T005 [US1] Event rows with level and sound in web/src/components/settings/AlertsTab.tsx

## Phase 4: User Story 2 - Only when relevant (P1)

- [X] T006 [US2] Darkness limits, restart seeding and settling in lib/AlertLogic/src/AlertEngine.cpp
- [X] T007 [US2] Alerts on/off via REST, MQTT, N.I.N.A. and UI in src/WebServer.cpp, web/src/components/AlertsBell.tsx, web/src/components/settings/AlertsTab.tsx

## Phase 5: User Story 3 - Own wording (P2)

- [X] T008 [US3] Variables and template rendering in src/WebServer.cpp and lib/AlertLogic/src/AlertEngine.cpp
- [X] T009 [US3] Wording editor and per-event Test in web/src/components/settings/AlertsTab.tsx

## Phase 6: User Story 4 - Channels and history (P2)

- [X] T010 [US4] Channel tests, recent list and clear in src/WebServer.cpp and web/src/components/AlertsBell.tsx

## Phase 7: Polish & Cross-Cutting

- [X] T011 [P] Alerts guide in docs/user-guide/alerts.md

## Phase 8: Convergence

- [X] T012 Offer and document the {event} variable the device already fills (missing from COMMON_VARS in web/src/components/settings/AlertsTab.tsx and the table in docs/user-guide/alerts.md) per FR-006 / SC-003 (partial)
- [X] T013 Show the device's actual default wording — "Skies clear" (not "Dark and clear") when sky alerts aren't limited to darkness — instead of a duplicated copy in DEFAULT_TEXT, in web/src/components/settings/AlertsTab.tsx per FR-006 / US3-AC2 (contradicts)
- [X] T014 Use the UI's labels in docs/user-guide/alerts.md: "App token" (not "API token") and "Skip certificate checks" (not "Skip TLS certificate checks"); describe the "Send alerts" master switch, per FR-009 / SC-004 (contradicts)
- [X] T015 Move the level → Pushover/ntfy priority mapping out of src/AlertDispatcher.cpp into lib/AlertLogic with tests per Constitution III (partial)
