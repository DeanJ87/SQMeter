# Implementation Plan: Alert schedule wording and "imaging app lost" alerts

**Branch**: `spec/021-alert-schedule` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/021-alert-schedule-and-client-watch/spec.md`

## Summary

Replace the "When you're not imaging" controls with one model: a persisted **sending mode**
(`any` / `whileConnected`) plus a live **pause state** that records its reason and since when.
Add per-device **client watch** to the shared Alpaca logic: it counts requests and clean
disconnects for the SafetyMonitor and ObservingConditions devices and turns silence into
`client_lost` / `client_back` / `client_disconnected` alert events, configured like every other
event.

All decision logic is new pure code in `lib/` (native-tested, and run by the demo through WASM):

- `lib/AlpacaLogic` `Router` gains clock-free per-device activity counters; new `ClientWatch`
  turns them into watching / silent / disconnected states on the device's uptime clock.
- `lib/AlertLogic` gains `AlertSchedule` (mode + pause state machine, reasons) and three client
  events in `AlertEngine` (cooldown and stacking as for other events).
- `lib/DeviceCore` maps the new event settings, template variables, sample alerts and writes the
  `/api/alerts/armed` and `/api/status` documents for both the firmware and the demo.
- `lib/ConfigModel` adds `alerts.sendMode`, the silence times and three event settings, migrates
  `armWithAlpaca`, keeps writing it, and persists the client-event settings under their own NVS
  key so the alerts JSON limit isn't exceeded (FR-022).

The firmware (`src/WebServer.cpp`) and demo bridge (`tools/demo-core/bridge.cpp`) replace their
duplicated arming code with `AlertSchedule`. The web UI gets the new wording, status line,
Pause/Resume button, event rows, the Alpaca client state, a wording test, and a self-contained
simulated imaging app for the Demo panel.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17 / PlatformIO; native and Emscripten builds of
`lib/`), TypeScript 5 strict (Preact + Vite).

**Primary Dependencies**: ArduinoJson 6, ESPAsyncWebServer, Unity (native tests), Vitest, MSW,
Playwright, Emscripten 6 (demo core).

**Storage**: NVS: main config JSON (5100 B), alerts JSON (3900 B), new client-alerts JSON key,
and the `sqm-alerts` namespace for the live pause state (`armed`, `reason`, `since`).

**Testing**: `pio test -e native` (new `test_alert_schedule`, client cases in `test_alpaca_logic`
and `test_alert_logic`, config migration in the config tests); Vitest (AlertsTab wording,
Alpaca page, bell); Playwright demo tests; contract schemas (Ajv in web tests,
`tools/contract-check.py`).

**Target Platform**: ESP32 (standard and BLE builds), browser demo (WASM).

**Project Type**: Embedded firmware + web UI + browser demo sharing `lib/`.

**Performance Goals**: "Stopped checking" within the silence time + 10 s (SC-001): the check runs
on every 1 s alert pass.

**Constraints**: No blocking in Alpaca handlers (counters only, no clock or allocation); flash
growth < 20 KB; persisted alerts JSON ≤ 3900 B with every template at its maximum.

**Scale/Scope**: 2 Alpaca devices, 3 new events, ~2 new settings, 1 mode setting.

## Constitution Check

| Principle | How this plan satisfies it |
|---|---|
| I. Fail-safe verdict | Nothing here touches the verdict; client watch and the schedule only affect notifications. Alerts still can't change what Alpaca serves. |
| II. Alpaca conformance | Router behaviour on the wire is unchanged: counters are recorded after the existing handling. ConformU runs in CI against `tools/alpaca-sim` (same Router). |
| III. Testable pure logic | `ClientWatch`, `AlertSchedule`, client events and the config migration are pure `lib/` code with native tests; UI wording has a Vitest test. |
| IV. Resource budgets | Counters are `uint32_t`s; client events add 3 `EventSetting`s. Alerts JSON split keeps both NVS strings under their limits. Flash delta reported in the PR. |
| V. Quiet, consistent UI | Reuses `SettingsCard`, `Group`, `Field`, `SelectInput`, `ActionButton`, `Note`, `Requires`. The client events show "Alpaca is off" with a fix link when Alpaca is disabled. |
| VI. Trusted-LAN security | `/api/alerts/arm` and `/disarm` keep `requireAuth`; the new `source` parameter is informational. |
| VII. Docs move with behaviour | `docs/user-guide/alerts.md`, `alpaca.md`, `mqtt.md`, `docs/api/rest.md`, `docs/user-guide/configuration.md` updated. |

No violations. Post-design re-check: unchanged (the split NVS key is within Principle IV's
intent; recorded in research.md D5).

## Project Structure

### Documentation (this feature)

```text
specs/021-alert-schedule-and-client-watch/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── rest.md          # /api/alerts/armed, arm/disarm, /api/status additions, /api/config alerts
│   └── device-core.md   # demo core bindings changed (setArmed source, statusParts)
└── tasks.md
```

### Source Code (repository root)

```text
lib/AlpacaLogic/include/AlpacaRouter.h     # DeviceActivity counters
lib/AlpacaLogic/src/AlpacaRouter.cpp
lib/AlpacaLogic/include/ClientWatch.h      # new: watching / silent / disconnected per device
lib/AlpacaLogic/src/ClientWatch.cpp
lib/AlertLogic/include/AlertSchedule.h     # new: send mode + pause state and reasons
lib/AlertLogic/src/AlertSchedule.cpp
lib/AlertLogic/include/AlertEngine.h       # client events, inputs, rules
lib/AlertLogic/src/AlertEngine.cpp
lib/ConfigModel/include/Config.h           # sendMode, silence times, 3 event settings
lib/ConfigModel/src/ConfigModel.cpp        # defaults, validation, parse/serialize, migration, split JSON
lib/DeviceCore/include/DeviceCore.h        # client inputs, vars, samples, document writers
lib/DeviceCore/src/DeviceCore.cpp
src/ConfigStore.cpp                        # client-alerts NVS key
src/WebServer.cpp, include/WebServer.h     # AlertSchedule, ClientWatch wiring, routes, status
tools/demo-core/bridge.cpp                 # same wiring for the demo
test/test_alert_schedule/                  # new native suite
test/test_alpaca_logic/, test/test_alert_logic/, test/test_config*/  # extended
web/src/types/index.ts, components/settings/{AlertsTab,defaults}.tsx, validation/configSchema.ts
web/src/components/{AlertsBell,SafetyCard,Alpaca}.tsx
web/src/demo/{DemoImagingApp.tsx,device.ts,handlers.ts,DemoPanel.tsx}
web/src/mocks/{data,handlers}.ts
specs/016-demo-device-emulation/contracts/schemas/{status,config}.schema.json
docs/user-guide/{alerts,alpaca,mqtt,configuration}.md, docs/api/rest.md
```

**Structure Decision**: Existing layout; new pure modules beside their siblings in `lib/`.

## Complexity Tracking

None.
