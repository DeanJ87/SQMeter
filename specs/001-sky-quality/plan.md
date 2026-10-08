# Implementation Plan: Sky Quality Measurement

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-sky-quality/spec.md`

**Note**: As-built plan. It records how the feature is implemented today so `/speckit-converge`
can compare code, docs and UI with the spec.

## Summary

The TSL2591 is sampled every ~600 ms. Raw counts go into a rolling buffer
(`skyAveraging.windowSeconds`); the averaged visible count minus the dark offset is converted
to lux and then to SQM, with an optional SQM offset. In daylight the sensor auto-ranges gain and
integration. NELM and Bortle are derived from the SQM. Results flow to `/api/sensors`,
`/ws/sensors`, the dashboard, Alpaca ObservingConditions, MQTT and the safety rules.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17); TypeScript (Preact)
**Primary Dependencies**: Adafruit TSL2591 library, ArduinoJson 6, ESPAsyncWebServer
**Storage**: NVS (config: `skyAveraging`, `skyCalibration`)
**Testing**: Unity native tests (`pio test -e native`), Vitest
**Target Platform**: ESP32 (standard and Bluetooth builds); browser
**Project Type**: Embedded firmware + web UI
**Performance Goals**: ~600 ms sampling; averaging window 10–300 s
**Constraints**: Night readings at MAX gain / 600 ms; no saturation crash in daylight
**Scale/Scope**: One sensor per device

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe safety | A faulted light sensor must not feed a real-looking SQM into the safety rules | Sensor status gates the SQM rule (feature 006) |
| III. Testable pure logic | SQM/NELM/Bortle conversion lives in `lib/*` with native tests | Lives in `src/calculations/SkyQuality.cpp`; no native tests |
| IV. Resource budgets | Fixed-size sample buffer | Fixed buffers sized for 300 s |
| V. Quiet UI | Shared components; hidden when not detected | Dashboard card uses shared components |
| VII. Docs | Formulas and defaults documented identically | Sky-quality reference, configuration reference, ADR-001 |

## Project Structure

### Documentation (this feature)

```text
specs/001-sky-quality/
├── spec.md
├── plan.md
├── tasks.md
└── checklists/requirements.md
```

### Source Code (repository root)

```text
src/sensors/TSL2591Sensor.cpp         # sampling, auto-range, rolling average, dark/SQM offset
include/sensors/TSL2591Sensor.h
src/calculations/SkyQuality.cpp       # lux→SQM, NELM, Bortle, descriptions
include/calculations/SkyQuality.h
src/WebServer.cpp                     # /api/sensors + /ws/sensors payload (lightSensor, skyQuality,
                                      # lightDiagnostics), POST /api/sensors/tsl2591/calibrate-dark
src/Config.cpp, include/Config.h      # skyAveraging, skyCalibration (defaults, validation, JSON)
web/src/components/Dashboard.tsx      # Sky Quality hero card, trend sparkline
web/src/components/settings/SensorsTab.tsx  # Sky sensors card
web/src/types/index.ts, web/src/validation/configSchema.ts
docs/reference/sky-quality.md         # formulas, Bortle table
docs/development/adr-001-sky-optics-and-averaging.md
docs/user-guide/configuration.md      # skyAveraging, skyCalibration fields
docs/api/rest.md, docs/api/websocket.md
web/tests/screenshots.spec.ts         # dashboard screenshot for the docs
```

**Structure Decision**: Single firmware project (`src/`, `include/`, `lib/`) with the web UI
in `web/`; documentation in `docs/`.

## Complexity Tracking

No justified violations. The Principle III gap above is a finding, not an accepted exception.
