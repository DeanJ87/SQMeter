# Implementation Plan: Wind (Anemometer and Vane)

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/004-wind/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`WindSensor` counts debounced anemometer pulses in an ISR and samples the vane ADC once a second;
`WindAggregator` (lib/WindLogic, natively tested) keeps 600 s of history and computes the 2-minute
mean, 3-second gust and circular-mean direction. Config changes reconfigure the sensor live.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: ESP32 GPIO interrupts and ADC1
**Storage**: NVS (`wind`, `alpaca.wind*`)
**Testing**: Unity native tests (`test_wind_logic`), Vitest
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI
**Constraints**: Vane on ADC1 (GPIO 32–39); pins clear of 16–19, 21, 22
**Scale/Scope**: One anemometer and vane

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe | Wind limit without a working anemometer is unsafe | Implemented (feature 006) |
| III. Testable pure logic | Aggregation in `lib/` with tests | `lib/WindLogic` + `test_wind_logic` |
| VII. Docs | Settings named as in the UI | `docs/hardware/wind.md` |

## Project Structure

### Documentation (this feature)

```text
specs/004-wind/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
lib/WindLogic/src/WindAggregator.cpp, lib/WindLogic/include/*.h   # speed/gust/direction
test/test_wind_logic/test_main.cpp
src/sensors/WindSensor.cpp            # ISR pulse counting (2 ms debounce), vane ADC
src/main.cpp                          # live reconfigure on config save
src/Config.cpp                        # wind config + pin validation
src/WebServer.cpp                     # wind payload
web/src/components/Dashboard.tsx      # Wind card
web/src/components/settings/SensorsTab.tsx   # Wind card (Model, Speed per pulse, vane)
web/src/components/settings/SafetyTab.tsx    # Wind limits
docs/hardware/wind.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
