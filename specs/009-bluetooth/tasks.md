---
description: "As-built task list (backfill) for Bluetooth"
---

# Tasks: Bluetooth (BLE Build)

**Input**: Design documents from `specs/009-bluetooth/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 BLE build env and partition layout in platformio.ini and partitions_ble.csv

## Phase 2: Foundational

- [X] T002 Payloads and alarm state machine in lib/BleLogic/ with tests

## Phase 3: User Story 1 - Wake me on my phone (P1)

- [X] T003 [US1] Secured alarm/ack/heartbeat characteristics in src/BleService.cpp
- [X] T004 [US1] Wake-me events raise alarms in src/WebServer.cpp
- [X] T005 [US1] Passkey, ack and unpair in web/src/components/settings/DeviceTab.tsx

## Phase 4: User Story 2 - Readings without WiFi (P3)

- [X] T006 [US2] Advertising and read/notify characteristics in src/BleService.cpp

## Phase 5: Polish & Cross-Cutting

- [X] T007 [P] Bluetooth guide in docs/user-guide/ble.md

## Phase 6: Convergence

- [ ] T008 Update docs/user-guide/ble.md install step: "Settings → Bluetooth (BLE) → Enable Bluetooth" is now Settings → Device → Bluetooth → "Turn on Bluetooth" per FR-005 / SC-002 (contradicts)
- [ ] T009 Clarify in docs/user-guide/ble.md that phones ring when "Send alerts" is off but not when alerts are switched off ("Alerts on now" / not imaging) per FR-003 / FR-005 (contradicts)
