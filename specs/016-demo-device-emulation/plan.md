# Implementation Plan: Demo That Behaves Like the Device

**Branch**: `feat/demo-emulation` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/016-demo-device-emulation/spec.md`

## Summary

Replace the demo's canned mock answers with one emulated device that runs the firmware's own logic,
compiled to WebAssembly, fed by a small sky simulator with weather scenarios. First move the remaining
decision logic and document builders out of `src/` into `lib/` (no behaviour change), then build the
device core for the browser, wire the existing mock layer to it, serve direct API URLs via `404.html`,
add the demo panel (scenarios, Reset), and lock it down with network-isolation, link and contract tests.

## Technical Context

**Language/Version**: C++17 (firmware `lib/`, also compiled to WebAssembly); TypeScript 5 / Preact (web UI)

**Primary Dependencies**: ArduinoJson 6 (header-only); Emscripten (pinned) with embind; MSW 2 (demo
request layer); Vite; Playwright; Ajv (schema checks in tests)

**Storage**: `sessionStorage` in the visitor's browser only

**Testing**: PlatformIO native (Unity) for `lib/`; Vitest for the web/demo; Playwright for links,
network isolation and screenshots; ConformU for Alpaca

**Target Platform**: Static site (GitHub Pages, `demo.sqmeter.dev`); firmware ESP32 unchanged in behaviour

**Project Type**: Embedded firmware + web UI (single repo)

**Performance Goals**: Settings take effect in the demo within 5 s (SC-003); demo first load under
~2 s on broadband (core is lazy-loaded with the demo)

**Constraints**: No network requests except the demo's own files (FR-006); device LittleFS image must
not grow (demo code is demo-build only); firmware flash/heap unchanged within noise (Constitution IV)

**Scale/Scope**: ~10 `src/` functions move to `lib/`; one WASM module; ~25 mocked endpoints rerouted

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Fail-safe safety | Device safety logic is moved, not changed; native tests and ConformU must pass unchanged | ✅ |
| II. Alpaca conformance | `Alpaca::Router` untouched; ConformU runs against alpaca-sim and a device after the moves | ✅ |
| III. Testable pure logic in lib/ | Strengthened: config validation, night/alert glue and document builders move into `lib/` with native tests | ✅ |
| IV. Resource budgets | Firmware: moved code only; measure flash and heap. Web: demo core loaded only in the demo build | ✅ (measure) |
| V. Quiet, consistent UI | Demo panel uses `ui.tsx`/`controls.tsx` building blocks; works at phone width | ✅ |
| VI. Trusted-LAN security | Demo makes no outbound requests (CSP + test); nothing implies a device is internet-safe | ✅ |
| VII. Docs move with behaviour | `docs/live-demo.md` rewritten (scenarios, what's simulated); contributing notes for the core | ✅ |
| Platform constraints | Adds Emscripten as a build tool for the demo only (not on the device) | ⚠ see Complexity |

Post-design re-check: unchanged - the design keeps the firmware's behaviour fixed and puts all new
code in `lib/`, `tools/demo-core/` and `web/src/demo/`.

## Project Structure

### Documentation (this feature)

```text
specs/016-demo-device-emulation/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── device-core.md
│   ├── demo-urls.md
│   └── schemas/          # JSON Schemas (written in tasks): readings, status, safety, config, alpaca-management
└── tasks.md              # /speckit-tasks
```

### Source Code (repository root)

```text
lib/
├── ConfigModel/          # NEW: Config struct, defaults, JSON, validate (from src/Config.cpp)
└── DeviceCore/           # NEW: snapshot → readings/safety/status docs, night, alert vars/templates,
                          #      Alpaca backend (from src/WebServer.cpp)
src/
├── Config.cpp            # NVS load/save only
└── WebServer.cpp         # routes + hardware; calls DeviceCore
test/
├── test_config_model/    # NEW
└── test_device_core/     # NEW
tools/
├── demo-core/            # NEW: embind bridge (EmulatedDevice), build script, pinned Emscripten
└── contract-check.py     # NEW: validate a real device against the schemas
web/
├── src/demo/             # NEW (demo build only)
│   ├── core/             # generated sqm-core.mjs + .wasm (committed; CI checks freshness)
│   ├── device.ts         # EmulatedDevice wrapper, persistence, clock
│   ├── simulator.ts      # sky/sensor inputs, scenarios
│   ├── handlers.ts       # MSW handlers → device (replaces src/mocks/handlers.ts for the demo)
│   ├── ApiView.tsx       # direct API URL view (404.html)
│   └── DemoPanel.tsx     # scenarios, time multiplier, Reset demo
├── src/mocks/            # unit-test fixtures stay; demo no longer uses canned answers
└── tests/
    ├── demo-links.spec.ts
    ├── demo-consistency.spec.ts
    └── demo-network.spec.ts
```

**Structure Decision**: Single repository; shared C++ in `lib/` is the one implementation for firmware,
native tests, alpaca-sim and the demo core.

## Phases

1. **Move to lib (no behaviour change)**: `ConfigModel`, `DeviceCore`; firmware calls them; native tests;
   ConformU; device check. Ships on its own.
2. **Device core for the browser**: `tools/demo-core` bridge + build, committed output, CI freshness check.
3. **Demo on the core**: `web/src/demo/*`; MSW handlers route to the core; persistence; simulated
   outbound actions; `404.html` API view and setup redirects. Fixes US1 + US2 + US3.
4. **Demo panel and scenarios**: weather/sensor scenarios, time multiplier, Reset demo. US5.
5. **Contracts and isolation tests**: JSON Schemas, Vitest/Ajv, `contract-check.py`, Playwright link /
   consistency / network tests, CSP. US4, SC-001/002/004/005.
6. **Docs**: `docs/live-demo.md` (what's real, what's simulated, scenarios), contributing (core build),
   screenshots regenerated from the emulated device.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Emscripten added as a build tool (demo only) | FR-004: the demo must run the device's own rules | A TypeScript port would be a second implementation that drifts (the problem FR-010 exists for); generated output is committed so web contributors don't need it |
