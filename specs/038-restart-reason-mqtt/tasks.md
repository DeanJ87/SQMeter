# Tasks: Restart reason over MQTT

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Foundational

- [x] T001 [US1] Add `DeviceInfo`, `restartReasonName()` and `writeDevice()` to `lib/Readings/include/Readings.h` and `lib/Readings/src/Readings.cpp` (FR-001, FR-003)
- [x] T002 [P] [US1] Native tests for every reason name, unknown codes, null boot time and the ISO boot time in `test/test_readings/test_device.cpp`

## Phase 2: User Story 1 - See why and when the device last restarted (P1) 🎯 MVP

- [x] T003 [US1] Publish retained `<base>/device` after each MQTT connection, and again once when the clock first becomes valid, in `src/WebServerStatus.cpp` / `src/WebServer.h` (FR-001, FR-002, FR-005)

## Phase 3: User Story 2 - A history of restarts (P2)

- [x] T004 [US2] Announce "Last restart reason" and "Last boot" as diagnostic entities on `<base>/device` in `lib/Readings/src/Readings.cpp`; removal follows discovery off (FR-004)
- [x] T005 [P] [US2] Discovery tests: both entities, `entity_category` diagnostic, the timestamp device class, the state topic, removal; update the message count in `test/test_readings/test_main.cpp`

## Phase 4: Polish

- [x] T006 Document the topic, fields, names and the brownout caveat in `docs/user-guide/mqtt.md` (FR-006)
- [x] T007 Native tests and the firmware builds (esp32dev, esp32-s3) pass; sketch size checked against the main device's slot

## Dependencies & Execution Order

T001 → T002, T003, T004 → T005 → T006 → T007.
