---
description: "As-built task list (backfill) for Time, Location and Sun & Moon"
---

# Tasks: Time, Location and Sun & Moon

**Input**: Design documents from `specs/005-time-location/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 TimeManager with POSIX TZ and NTP/GPS sync in src/TimeManager.cpp

## Phase 2: Foundational

- [X] T002 Time, GPS and location config with validation in src/Config.cpp

## Phase 3: User Story 1 - Correct time (P1)

- [X] T003 [US1] Time zone list/custom and source priority in web/src/components/settings/TimeTab.tsx

## Phase 4: User Story 2 - Darkness from where I am (P1)

- [X] T004 [US2] Device sun elevation in lib/AlertLogic/src/SunPosition.cpp with tests in test/test_sun_position/test_main.cpp
- [X] T005 [US2] computeNight and /api/status sky in src/WebServer.cpp
- [X] T006 [US2] Coordinates field, paste parsing and secure-context "Use my location" in web/src/components/settings/TimeTab.tsx
- [X] T007 [US2] Darkness readout in web/src/components/settings/AlertsTab.tsx

## Phase 5: User Story 3 - See tonight at a glance (P2)

- [X] T008 [US3] Browser sun/moon astronomy in web/src/lib/astro.ts with tests
- [X] T009 [US3] Sun & Moon card and night chart in web/src/components/SunMoonCard.tsx and web/src/components/NightChart.tsx

## Phase 6: Polish & Cross-Cutting

- [X] T010 [P] Location section in docs/user-guide/configuration.md

## Phase 7: Convergence

- [ ] T011 Show the device's sun altitude and dark/not-dark (status.sky) in the Alerts tab darkness readout, keeping browser predictions only for start/end times, in web/src/components/settings/AlertsTab.tsx per FR-005 / US2-AC2 (partial)
- [ ] T012 Remove or wire up the stored-but-unused settings `timezone` (top level, "Display timezone"), `ntp.gmtOffsetSec` and `ntp.daylightOffsetSec` in src/Config.cpp and docs/user-guide/configuration.md per FR-008 / SC-003 (unrequested)
- [ ] T013 Show Sun & Moon and darkness times in the device's time zone (or label them as browser time) in web/src/components/SunMoonCard.tsx, web/src/components/NightChart.tsx and web/src/components/settings/AlertsTab.tsx per FR-007 (partial)
