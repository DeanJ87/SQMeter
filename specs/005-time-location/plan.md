# Implementation Plan: Time, Location and Sun & Moon

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/005-time-location/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`TimeManager` applies the POSIX `TZ`, syncs NTP and/or GPS time by priority. The device computes
sun elevation (NOAA approximation, `lib/AlertLogic/src/SunPosition.cpp`) from a GPS fix or the
saved location and reports it in `/api/status` → `sky`. The browser computes sun and moon positions
(`web/src/lib/astro.ts`) for the Sun & Moon card, the night chart and the Alerts tab's darkness
predictions.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: ESP32 SNTP, TinyGPS++
**Storage**: NVS (`ntp`, `gps`, `location`, `primaryTimeSource`, `secondaryTimeSource`, `timezone`)
**Testing**: Unity (`test_sun_position`), Vitest (`web/src/lib/__tests__/astro.test.ts`, SunMoonCard tests)
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI
**Constraints**: GPS on 16/17
**Scale/Scope**: One location

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| III. Testable pure logic | Astronomy in `lib/` / `web/src/lib` with tests | Both tested |
| V. Quiet UI | Shared components | Yes |
| VII. Docs | Time/location settings documented as they work | Configuration reference |

## Project Structure

### Documentation (this feature)

```text
specs/005-time-location/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/TimeManager.cpp                     # TZ, NTP/GPS sync, priority
src/sensors/GPSSensor.cpp
lib/AlertLogic/src/SunPosition.cpp      # device sun elevation
test/test_sun_position/test_main.cpp
src/WebServer.cpp                       # computeNight, /api/status sky
src/Config.cpp                          # ntp, gps, location, timezone, priorities
web/src/lib/astro.ts                    # browser sun/moon, crossings, darkness
web/src/components/SunMoonCard.tsx, web/src/components/NightChart.tsx
web/src/components/settings/TimeTab.tsx # time zone, sources, location, card toggle
web/src/components/settings/AlertsTab.tsx  # darkness readout
docs/user-guide/configuration.md        # Device/NTP/GPS/Location fields
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
