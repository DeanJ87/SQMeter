# Implementation Plan: Settings Dependencies

**Branch**: `spec/020-settings-dependencies` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/020-settings-dependencies/spec.md`

## Summary

Every setting that only works when something else is on gets one answer to "is this in effect?", decided by the device's own logic and shown the same way everywhere. A new pure library, `lib/SettingsDeps`, evaluates the catalogue against the config and the device's runtime facts and produces the effective-state report (`GET /api/settings/effective`). The firmware stops acting on inactive settings (alert channels, arming, Home Assistant's alerts switch), the demo runs the same evaluator via WASM, and the web UI shows each dependent setting as off / active / inactive (with reason and a one-click fix) / unknown, previewing unsaved drafts with a TypeScript mirror that is held to the device's answers by shared fixtures. A CI checker keeps the catalogue, code markers, tests and docs in step. Decisions: [research.md](research.md).

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17 / PlatformIO; native for tests; Emscripten for the demo), TypeScript strict (Preact + Vite), Python 3 stdlib (checker)

**Primary Dependencies**: ArduinoJson 6, ESPAsyncWebServer, Unity (native tests), Vitest + Testing Library + MSW, Playwright (demo)

**Storage**: none new - the report is derived; config storage unchanged (NVS limits untouched)

**Testing**: `pio test -e native` (new `test/test_settings_deps`), Vitest (evaluator parity + Settings UI), Playwright demo test, `tools/settings-deps/test_check.py`

**Target Platform**: ESP32 (standard + BLE builds), browser (device UI and demo)

**Project Type**: embedded firmware + web UI + WASM demo

**Performance Goals**: evaluation is O(catalogue) (~40 rules) - microseconds; run on request and once per alert pass

**Constraints**: flash +≤ 12 KB per firmware image (measured +10.1 KB standard, +10.2 KB Bluetooth); report JSON < 5 KB (`Deps::reportCapacity`, strings referenced not copied); no new heap in the 1 s loop beyond one small mask computation; UI bundle +≤ 4 KB gzip

**Scale/Scope**: 36 catalogue entries (32 dependencies incl. new D-36, 3 constraints, D-32 classified as a dependency); 6 settings tabs

## Constitution Check

| Principle | How this plan satisfies it |
|---|---|
| I. Fail-safe verdict | No change to the verdict. Unmet behaviour of each safety rule recorded from current code (R4); `rulesNotInEffect` is informational and additive. Alerts still can't change the verdict. |
| II. Alpaca | No Alpaca router changes. D-21/D-22 stay NotImplemented as today. |
| III. Testable pure logic | Evaluator in `lib/SettingsDeps`, native-tested; UI covered by Vitest; parity fixtures shared. |
| IV. Budgets | Flash measured for both images; report endpoint uses a bounded document on the async task (no network, no blocking); no new persisted config. |
| V. Quiet, consistent UI | Reuses `Toggle`/`Requires`/`Note`; disabled-with-reason-and-fix for every catalogued dependency (this principle, applied everywhere). |
| VI. Security | New endpoint is read-only and reveals no secrets (same exposure as `/api/status`). Test sends keep `requireAuth`. |
| VII. Docs | User guide notes + REST reference for the endpoint and `rulesNotInEffect`; generated reference page for the catalogue; mkdocs strict. |
| Compatibility | Additive API; no config removed or rejected that loaded before (D-32 stays a dependency, R5). |

No violations - Complexity Tracking not needed.

## Project Structure

### Documentation (this feature)

```text
specs/020-settings-dependencies/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── effective-settings.md        # GET /api/settings/effective (schema: specs/016-…/contracts/schemas/settings-effective.schema.json)
└── tasks.md
```

### Source Code

```text
lib/SettingsDeps/
├── catalogue.json                 # source of truth (IDs, chains, reasons, fixes)
├── include/SettingsDeps.h         # Facts, Entry, evaluate(), writeReport(), rulesNotInEffect()
└── src/SettingsDeps.cpp
lib/DeviceCore/                    # Core::sensorFacts(); writeSafety adds rulesNotInEffect
lib/Readings/                      # discovery: alerts switch only while alerts are on
src/WebServer.cpp                  # facts, /api/settings/effective, active channel mask, arming guard
src/AlertDispatcher.cpp/.h         # skipped-with-reason for inactive channels
tools/demo-core/bridge.cpp         # effective(), same channel/arming rules
tools/settings-deps/check.py       # CI checker (+ test_check.py)
test/test_settings_deps/           # native tests
test/fixtures/settings-deps/       # cases.json, default-config.json (shared with Vitest)
web/src/lib/settingsDeps.ts        # TS mirror for previews + catalogue texts/fix labels
web/src/components/settings/       # tabs use the effective view; preview in save bar
web/src/demo/handlers.ts, web/src/mocks/handlers.ts   # the new endpoint
docs/reference/settings-dependencies.md, docs/api/rest.md, docs/user-guide/*
.github/workflows/build.yml        # run the checker
```

**Structure Decision**: a new `lib/` library (pure, no Arduino) so the firmware, native tests and the demo share it, following the existing `lib/*` layout.

## Phases

1. **Foundation**: catalogue JSON, evaluator + facts + report writer, fixtures, native tests (parity source), TS mirror + Vitest parity.
2. **US1 (P1)**: firmware/demo stop acting on inactive settings (channels, arming, HA switch); endpoint; UI states, notes, fix actions, counts; safety `rulesNotInEffect`; constraint messages aligned.
3. **US2 (P1)**: round-trip test (off → save → restart → on → unchanged & active) in native and demo/Playwright.
4. **US3 (P2)**: UI uses the device's report; demo exposes it; agreement test.
5. **US4 (P2)**: checker + CI + markers + docs generation.
6. **US5 (P3)**: constraints shown before saving with the device's message.
7. **Polish**: docs, contracts/schemas, demo core rebuild, flash sizes, full test run.
