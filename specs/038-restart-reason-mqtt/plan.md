# Implementation Plan: Restart reason over MQTT

**Branch**: `feat/mqtt-restart-reason` | **Date**: 2026-11-24 | **Spec**: [spec.md](spec.md)

## Summary

Publish a retained `<base>/device` message (restart reason name and code, boot count, boot time)
after each MQTT connection and once more when the clock first becomes valid, and announce two
diagnostic Home Assistant entities that read it. The message building and the reason names are
pure functions in `lib/Readings` with native tests; the firmware only gathers the inputs and
publishes.

## Technical Context

**Language/Version**: C++17, Arduino-ESP32 3.x (ESP-IDF 5.5)
**Primary Dependencies**: ArduinoJson 6, the existing MQTT client
**Testing**: PlatformIO native Unity tests (`test/test_readings`)
**Target Platform**: ESP32 / ESP32-S3
**Constraints**: app slot ~79 KB free on the main device; one small message per connection
**Scale/Scope**: one topic, two discovery entities, one doc section

## Constitution Check

- **I Fail-safe verdict**: untouched; nothing here feeds the safety verdict. Pass.
- **II Alpaca conformance**: untouched. Pass.
- **III Testable pure logic**: `restartReasonName` and `writeDevice` live in `lib/Readings` with
  native tests; the firmware supplies `esp_reset_reason()`, `bootCount` and the clock. Pass.
- **IV Resource budgets**: one ~120-byte payload per connection; discovery adds two entities to
  the existing loop. Pass.
- **V Quiet UI**: no UI change. Pass.
- **VI Trusted-LAN security**: no new inputs; publish only. Pass.
- **VII Docs move with behaviour**: `docs/user-guide/mqtt.md` updated in the same PR. Pass.
- **VIII Code quality**: follows the existing Readings/discovery patterns. Pass.

## Project Structure

### Documentation (this feature)

```text
specs/038-restart-reason-mqtt/
├── spec.md
├── plan.md
├── tasks.md
└── checklists/requirements.md
```

### Source Code (repository root)

```text
lib/Readings/include/Readings.h   # DeviceInfo, restartReasonName, writeDevice, Group::Device entities
lib/Readings/src/Readings.cpp
src/WebServerStatus.cpp           # publish <base>/device after connect and on clock-valid
src/WebServer.h                   # publish bookkeeping
test/test_readings/test_device.cpp
docs/user-guide/mqtt.md
```

## Complexity Tracking

None.
