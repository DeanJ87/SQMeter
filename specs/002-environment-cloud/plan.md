# Implementation Plan: Environment and Cloud Cover

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-environment-cloud/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

The BME280 provides temperature, humidity and pressure; dew point is computed with the Magnus
formula. The MLX90614 provides sky (object) and ambient temperatures. `CloudDetection` turns
the humidity-corrected sky-minus-ambient delta into cloud cover and a condition using the
configured thresholds; without the BME280 a 53% humidity is assumed. Cloud metrics are computed
for the sensor payload, the Alpaca snapshot and the safety inputs.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17); TypeScript (Preact)
**Primary Dependencies**: Adafruit BME280, Adafruit MLX90614, ArduinoJson 6
**Storage**: NVS (`cloudDetection`)
**Testing**: Unity native tests, Vitest
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI
**Performance Goals**: Readings every `sensor.readIntervalMs` (default 5 s)
**Constraints**: Sensors on shared I2C (21/22)
**Scale/Scope**: One of each sensor

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe | Faulted IR sensor must not yield a real-looking cloud cover for safety | Safety skips the cloud rule on IR fault (feature 006) |
| III. Testable pure logic | Cloud model and dew point in `lib/*` with native tests | `src/calculations/CloudDetection.cpp`, `src/sensors/BME280Sensor.cpp`; no native tests |
| V. Quiet, consistent UI | Cards hidden for missing sensors; consistent units | Cards hidden; units shown as "C" |
| VII. Docs | Cloud model documented | Sky-quality reference (cloud section), configuration reference |

## Project Structure

### Documentation (this feature)

```text
specs/002-environment-cloud/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/sensors/BME280Sensor.cpp          # T/RH/P, Magnus dew point
src/sensors/MLX90614Sensor.cpp        # sky + ambient temperatures
src/calculations/CloudDetection.cpp   # humidity correction, cloud %, condition
src/WebServer.cpp                     # environment, irTemperature, cloudConditions payload;
                                      # Alpaca snapshot; safety inputs (3 CloudDetection call sites)
src/Config.cpp                        # cloudDetection defaults + validation
web/src/components/Dashboard.tsx      # Environment, Cloud Conditions, IR Temperature cards
web/src/components/settings/SensorsTab.tsx  # Cloud detection card
docs/reference/sky-quality.md         # cloud detection section
docs/user-guide/configuration.md      # cloudDetection fields
docs/api/rest.md, docs/api/websocket.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

No justified violations; Principle III and V gaps are findings.
