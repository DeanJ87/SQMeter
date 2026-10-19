# Implementation Plan: Two sky sensors (SQM and SQM-L)

**Branch**: `feature/v0.4-alpha` (spec branch `spec/030-dual-sky-sensors`) | **Date**: 2026-10-17 |
**Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

## Summary

Run two TSL2591s, the wide **SQM** on I2C-1 (`Wire`) and the narrow **SQM-L** on I2C-2 (`Wire1`),
through one driver class with per-sensor averaging and calibration. The driver's reads become
non-blocking and overlapped, and detect bus errors (today an unplugged sensor reads as darkness).
`lib/DeviceCore` keeps one **authoritative copy** of the sky reading (the existing `tsl`/`sky`
snapshot fields), chosen by `sky.authoritative` with no fallback, so every single-value consumer
(readings, safety, Alpaca, BLE, alerts, HA) switches source in one place. Both readings are also
published under their own names (`skySensors`, HA entities, alert variables, `/api/status`).
Config is additive and old configs load unchanged; the UI adds a name pill and two tiles to the Sky
card, a glance tile per sensor, and per-sensor rows and calibration cards in Settings.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5, pioarduino); TypeScript strict (Preact + Vite); Python 3 tools

**Primary Dependencies**: Adafruit TSL2591 1.4.x (begin/gain/timing only; channel reads done on
the `TwoWire` directly, research D2), ArduinoJson 6, PubSubClient, NimBLE (BLE build), Zod

**Storage**: NVS main config JSON (5100 B limit; now ~2.1-2.85 KB, +~330 B, research D5)

**Testing**: Unity native tests (`pio test -e native`), Vitest + MSW, Playwright (demo), contract
check against `specs/016-demo-device-emulation/contracts/schemas`, ConformU in CI

**Target Platform**: ESP32 (classic: I2C-2 unset by default) and ESP32-S3 Rev A (I2C-2 wired;
pins from the board-profile work); browser demo (WASM core)

**Project Type**: embedded firmware + embedded web UI + docs

**Performance Goals**: both sensors sampled on the existing ~650 ms cadence; main loop never blocks
for an integration (today it blocks ~720 ms per read)

**Constraints**: OTA slot (SIZE-04, expect < 6 KB growth), heap (+~3.3 KB only when SQM-L is
enabled), loop stack headroom > 2 KB, UI gzip growth < 10 KB (SIZE-02), readings document within
`Readings::DOC_CAPACITY` and the 3072-byte MQTT buffer, no change for one-sensor devices (SC-001)

**Scale/Scope**: 2 sensors max (third out of scope); ~20 firmware/lib files, ~15 web files, 10 docs pages

## Constitution Check

*Gate before Phase 0; re-checked after Phase 1 (below).*

| Principle | How this plan satisfies it | Status |
|---|---|---|
| I Fail-safe verdict | Authoritative sensor stays a required sensor; disabled/faulted/stale -> unsafe with a named reason; no fallback; non-authoritative sensor can't change the verdict; rain path untouched; reasons keep value + limit (D4). Fixes the latent "unplugged reads as dark" case (D2) | Pass |
| II Alpaca conformance | Same `Alpaca::Router`; only source + `SensorDescription` text change; NotImplemented when the authoritative sensor is disabled; ConformU in CI (D8) | Pass |
| III Testable pure logic | Role selection, read-cycle timing/back-off, config, readings, safety reasons, deps, alert vars all in `lib/*` with native tests; UI logic in Vitest; demo uses the same core | Pass |
| IV Resource budgets | Sizes measured (firmware, heap, config, readings doc, UI gzip); second sensor's buffers allocated only when enabled; non-blocking reads; device heap/stack measured on the spare device (D16) | Pass |
| V Quiet UI | Existing `Card`/`Pill`/`MetricTile`/`DepToggle`; no card/tile for disabled sensors; reasons behind "?" via settings deps; mockup reviewed before UI build (contracts/ui.md) | Pass (mockup review is a task gate) |
| VI Trusted-LAN security | Calibration route keeps `requireAuth`; no new secrets | Pass |
| VII Docs with behaviour | sky-quality, configuration, REST, WebSocket, MQTT, Alpaca, BLE, alerts, settings-dependencies, hardware pages in the same change (FR-016) | Pass |
| VIII Code quality | `tools/quality/check.py` (incl. DASH-01/02, DS, settings-deps, i18n checks); no baseline growth | Pass |
| Compatibility | Additive config with defaults; existing keys keep meaning; release notes mention new fields only | Pass |

## Project Structure

### Documentation (this feature)

```text
specs/030-dual-sky-sensors/
├── spec.md
├── plan.md              # this file
├── research.md          # D1-D16
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── config.md
│   ├── readings.md
│   ├── status-and-routes.md
│   ├── integrations.md  # HA, Alpaca, SafetyMonitor, BLE, alerts
│   └── ui.md
└── tasks.md
```

### Source Code (repository root)

```text
lib/SensorTypes/include/SensorTypes.h        SkyRole, names
lib/SkyLogic/                                SkyReadCycle (timing, failure count, re-begin back-off)
lib/ConfigModel/                             sky, skyAveragingL, skyCalibrationL, i2c2 pins; validation; JSON in/out
lib/DeviceCore/                              SensorSnapshot::skySensors[], authoritative copy, status skySensors,
                                             safety/Alpaca/alert inputs, deps facts
lib/Readings/                                sky.source, skySensors, HA entities sqm_wide/sqm_l/sqm_delta, DOC_CAPACITY
lib/AlpacaLogic/                             role-literal safety reasons, SensorDescription per role
lib/AlertLogic/                              SENSOR_COUNT 6, template-variables.json
lib/SettingsDeps/                            D-39..D-42, facts
include/sensors/TSL2591Sensor.h, src/sensors/TSL2591Sensor.cpp   bus + role, start/collect, fault detection
src/main.cpp                                 Wire1, two sensors, overlapped cycle
include/WebServer.h, src/WebServer*.cpp      snapshot of both, calibrate-dark ?sensor, BLE summary, doc sizes
tools/demo-core/                             second sensor feed; WASM rebuild
tools/api/operations.json, docs/api/openapi.json
specs/016-demo-device-emulation/contracts/schemas/*.schema.json
test/test_sky_logic, test_config_model, test_config_migration, test_device_core, test_readings,
test/test_alpaca_safety, test_alpaca_description, test_alert_variables, test_settings_deps, fixtures
web/src/types, validation, components/dashboard/skyCards.tsx, dashboard/glance.ts, inventory.json,
web/src/components/settings/SensorsSkyCards.tsx, defaults.ts, hooks/useSensorActions.ts,
web/src/lib/sensorNames.ts, settingsDeps.ts, alertVariables.ts, mocks/, demo/, i18n/
tools/i18n/glossary/en.json
docs/reference/sky-quality.md, docs/user-guide/{configuration,mqtt,alpaca,ble,alerts}.md,
docs/api/{rest,websocket}.md, docs/reference/settings-dependencies.md, docs/hardware/*, docs/getting-started/hardware.md
```

**Structure Decision**: the existing layout; the only new lib type is `SkyReadCycle` in
`lib/SkyLogic`. No new library directory.

## Phases

1. **Foundation**: `SkyRole`, config model + validation + migration test, `SkyReadCycle`
   (tests first).
2. **Core (US1/US2)**: DeviceCore authoritative copy and per-sensor snapshot, readings
   `sky.source`/`skySensors`, safety reasons, Alpaca, status, deps facts; then the driver, `main.cpp`
   and `WebServer` wiring; both firmware builds.
3. **Calibration (US3)**: route `?sensor=`, per-role reconfigure, Settings calibration cards.
4. **Integrations (US4)**: HA entities, alert variables and sensor slot, BLE summary.
5. **UI**: mockup and owner review, dashboard, glance, Settings, strings and translations.
6. **Demo, contracts, docs, budgets**: simulator + WASM rebuild, schemas, OpenAPI, docs pages,
   size/heap measurements, quality gate, convergence.

## Post-design Constitution re-check

Phase 1 artifacts keep every gate above: the contracts add only optional fields and keep one-sensor
output unchanged; safety reasons stay literal and value-bearing; the UI contract uses only shared
components and settings dependencies. No violations, so no Complexity Tracking entries.

## Complexity Tracking

None.
