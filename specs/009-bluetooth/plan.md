# Implementation Plan: Bluetooth (BLE Build)

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/009-bluetooth/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

The `esp32dev-ble` build (NimBLE, larger app slots) runs `BleService`: advertising, GATT
characteristics and the alarm service. `lib/BleLogic` (tested) holds payload encoding and the alarm
state machine. `processAlerts` raises alarms for wake-me events.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: NimBLE-Arduino 1.4
**Storage**: NVS (`ble`), NimBLE bond store
**Testing**: Unity (`test_ble_logic`), Vitest (DeviceTab)
**Target Platform**: ESP32 (BLE build)
**Project Type**: Embedded firmware + web UI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| III. Testable pure logic | Alarm logic in lib with tests | `lib/BleLogic` + tests |
| IV. Budgets | Fits 1.69 MB slot | Yes |
| VII. Docs | Settings paths current | docs/user-guide/ble.md |

## Project Structure

### Documentation (this feature)

```text
specs/009-bluetooth/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
lib/BleLogic/src/BleAlarm.cpp, lib/BleLogic/src/BlePayloads.cpp, test/test_ble_logic/
src/BleService.cpp, include/BleService.h
src/WebServer.cpp                  # processAlerts wake → raiseAlarm, /api/ble/ack, /api/ble/forget-bonds
web/src/components/settings/DeviceTab.tsx
partitions_ble.csv, platformio.ini [env:esp32dev-ble]
docs/user-guide/ble.md
```

**Structure Decision**: Optional build variant of the single firmware project.

## Complexity Tracking

None.
