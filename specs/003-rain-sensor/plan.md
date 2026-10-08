# Implementation Plan: Rain Sensor (Hydreon RG-15)

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/003-rain-sensor/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`RG15Sensor` talks to the RG-15 over UART1 (RX 18 / TX 19, 9600 baud) by polling with `R`,
parses the response line, maintains the latched `raining` state and the local event
accumulation, and handles daily total resets (`O`) and reboots (`K`). The web server exposes the
reading plus diagnostics in `rainSensor` (only when enabled) and `/api/status`; the dashboard
shows a Rain Sensor card; Sensors settings host the communication test and the System page the
reset/reboot actions.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17); TypeScript (Preact)
**Primary Dependencies**: HardwareSerial, ArduinoJson 6
**Storage**: NVS (`rain`)
**Testing**: Unity native tests, Vitest
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI
**Performance Goals**: Poll every `pollIntervalMs` (default 5 s)
**Constraints**: UART pins 18/19 reserved
**Scale/Scope**: One sensor

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe | Offline/stale/lens fault reported for safety | Reported; safety uses it (feature 006) |
| III. Testable pure logic | Rain latch (safety input) and response parsing in `lib/*` with tests | In `src/sensors/RG15Sensor.cpp`; no native tests |
| VI. Security | Actions require auth | test, reset-total, reboot call `requireAuth` |
| VII. Docs | Payload documented field-for-field | `docs/hardware/rg15.md` |

## Project Structure

### Documentation (this feature)

```text
specs/003-rain-sensor/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/sensors/RG15Sensor.cpp, include/sensors/RG15Sensor.h   # UART polling, parse, latch, resets
src/WebServer.cpp            # appendRG15Diagnostics, rainSensor payload, rg15 test/reset/reboot routes
src/Config.cpp               # rain config, validation
web/src/components/Dashboard.tsx              # Rain Sensor card
web/src/components/settings/SensorsTab.tsx    # Rain sensor card + communication test
web/src/components/System.tsx                 # reset total / reboot
lib/AlpacaLogic/src/ObservingConditionsMapper.cpp  # rainrate mm/h
docs/hardware/rg15.md, docs/user-guide/configuration.md, docs/api/rest.md, docs/api/websocket.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

No justified violations; Principle III gap is a finding.
