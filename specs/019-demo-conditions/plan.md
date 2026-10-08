# Implementation Plan: Demo Conditions - Set the Sensor Readings

**Branch**: `spec/019-demo-conditions` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/019-demo-conditions/spec.md`

## Summary

Replace the demo's outcome scenarios with the raw readings each simulated sensor reports, held as a
*conditions* model in the demo device. The firmware's own logic (the WASM device core, spec 016)
derives everything from them.

- **Shortcuts** become pure functions that compute input targets from the device's current
  settings. They report what they used, or why they can't apply.
- **The clock and location** gain presets. The device's POSIX time zone is evaluated in the demo,
  so local times are the device's own.
- **Waits:**
  - The device core gains a `pending()` document: sky averaging, the rain clear delay, and alert
    settle/cooldown/startup grace.
  - It also emulates the TSL2591 driver's rolling average, so the waits the panel shows are real.
- **The panel** is rebuilt as collapsible sensor groups with labelled inputs, derived readings and
  waits. On phones it's a scrolling bottom sheet.

## Technical Context

**Language/Version**: TypeScript 5 / Preact (demo panel, simulator); C++17 (`lib/AlertLogic`,
`tools/demo-core` bridge compiled to WebAssembly)

**Primary Dependencies**: existing only - Emscripten (brew) with embind, MSW 2, Vite, Vitest,
Playwright; no new runtime libraries

**Storage**: `sessionStorage` (`sqm.demo.v1` → `sqm.demo.v2`, migrated)

**Testing**: Vitest (conditions, shortcuts, presets, time zones); native Unity (alert engine
waits); Playwright (panel behaviour, scenario links, phone layout, network isolation)

**Target Platform**: the static demo site (`demo.sqmeter.dev`); firmware unchanged in behaviour

**Project Type**: Embedded firmware + web UI (single repo)

**Performance Goals**: a 1 s device tick stays well under a frame budget; the panel re-renders
on each tick only

**Constraints**:
- Nothing outbound (spec 016 FR-006).
- The device LittleFS image doesn't grow: demo code stays in the demo build.
- Firmware flash unchanged within noise: the new `AlertEngine::waits()` is unused on the device
  and stripped.

**Scale/Scope**:
- About 20 inputs across 6 sensors, 7 shortcuts, 7 time presets and 6 location presets.
- 1 new core function.
- About 6 Playwright scenarios.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Fail-safe safety | No change to the verdict logic; the demo feeds raw readings to the unchanged evaluator | ✅ |
| II. Alpaca conformance | `Alpaca::Router` untouched | ✅ |
| III. Testable pure logic | Alert waits are a pure `AlertEngine` query in `lib/` with native tests. Shortcuts, presets and the POSIX time-zone evaluator are pure TS with Vitest. | ✅ |
| IV. Resource budgets | Device code unchanged except one unused const query (stripped); both images built and sizes reported | ✅ (measure) |
| V. Quiet, consistent UI | Panel uses `Button`, `Note` and existing form classes (`input`, `field`); facts plainly; rain/wind controls disabled with reason and link | ✅ |
| VI. Trusted-LAN security | No requests leave the demo (Playwright isolation test extended to every control) | ✅ |
| VII. Docs move with behaviour | `docs/live-demo.md` rewritten for inputs, shortcuts, waits and links | ✅ |

Post-design re-check: unchanged.
- **Driver emulation:** the bridge's light averaging copies the TSL2591 driver's sliding window
  (`src/sensors/TSL2591Sensor.cpp`). It's emulation of hardware-driver behaviour in the demo-only
  bridge, not new decision logic. The decision logic (`luxToSQM`, cloud detection, safety) stays
  the shared `lib/` code.

## Project Structure

### Documentation (this feature)

```text
specs/019-demo-conditions/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── core-pending.md      # EmulatedDevice.pending() document
│   └── demo-state.md        # sessionStorage v2 + ?scenario= mapping
└── tasks.md
```

### Source Code (repository root)

```text
lib/AlertLogic/
├── include/AlertEngine.h      # + Wait, AlertEngine::waits() (const query)
└── src/AlertEngine.cpp
test/test_alert_logic/          # + waits tests
tools/demo-core/bridge.cpp      # light rolling average (driver emulation); pending()
web/src/demo/
├── conditions.ts      # NEW: inputs model, ranges, defaults, ramps, variation → core inputs
├── shortcuts.ts       # NEW: shortcuts computed from config (reachable / reason / used)
├── presets.ts         # NEW: time presets, location presets, sun crossings
├── posixTz.ts         # NEW: POSIX TZ evaluation (offset at an instant, local → UTC)
├── simulator.ts       # reduced: sky light from the sun; scenario links → presets/shortcuts
├── device.ts          # conditions, clock set, location apply, pending, persistence v2
├── handlers.ts        # status time in the device's zone (+hhmm)
├── DemoPanel.tsx      # rebuilt: groups, derived, waits, shortcuts, time & place
└── __tests__/         # conditions, shortcuts, presets, posixTz tests
web/src/index.css      # demo panel groups, phone bottom sheet
web/tests/demo.spec.ts # updated + new scenarios
docs/live-demo.md
```

**Structure Decision**: All new behaviour is demo-only, apart from the alert-wait query. That
query sits in `lib/AlertLogic` so it's the same code the device would use to report waits later.

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| POSIX TZ evaluator in TS | FR-014/017: the device's time zone, not the browser's, must drive local time; presets set POSIX rules | `Intl` needs IANA names; the device stores POSIX strings |
| Light averaging in the bridge | FR-007: the panel must show a real averaging wait; today the demo applies light instantly, unlike the device | Showing a fake countdown would contradict FR-005 |
