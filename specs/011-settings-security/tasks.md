---
description: "As-built task list (backfill) for Settings, Configuration and Security"
---

# Tasks: Settings, Configuration and Security

**Input**: Design documents from `specs/011-settings-security/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 Config persistence (NVS main + alerts keys), defaults and migration in src/Config.cpp

## Phase 2: Foundational

- [X] T002 Device validation in src/Config.cpp; browser validation in web/src/validation/configSchema.ts

## Phase 3: User Story 1 - Find and change a setting (P1)

- [X] T003 [US1] Six tabs, shared controls, dependency blocking in web/src/components/settings/
- [X] T004 [US1] Save bar and restart toasts in web/src/components/Settings.tsx and web/src/components/settings/restart.ts

## Phase 4: User Story 2 - Settings that don't break the device (P1)

- [X] T005 [US2] Live application of saved settings in src/main.cpp

## Phase 5: User Story 3 - Keep others out (P2)

- [X] T006 [US3] requireAuth on protected routes and secret masking in src/WebServer.cpp and src/Config.cpp

## Phase 6: Polish & Cross-Cutting

- [X] T007 [P] docs/user-guide/configuration.md and docs/user-guide/security.md

## Phase 7: Convergence

- [ ] T008 Make browser validation match the device: add the device's 24 h maximum to mqtt.publishIntervalMs and ntp.syncIntervalMs in web/src/validation/configSchema.ts, and let the MQTT "Publish every" input go to 86400 s (now 3600) in web/src/components/settings/NetworkTab.tsx, per FR-004 / SC-001 (contradicts)
- [ ] T009 Add browser validation for skyCalibration.sqmOffset (±5) and skyAveraging.windowSeconds (10–300) alongside the calibration UI (see 001 T016) in web/src/validation/configSchema.ts per FR-004 (partial)
