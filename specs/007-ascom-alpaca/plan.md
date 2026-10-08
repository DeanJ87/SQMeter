# Implementation Plan: ASCOM Alpaca Devices

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/007-ascom-alpaca/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`Alpaca::Router` (lib/AlpacaLogic) implements the management and device APIs independent of the
web server; the firmware feeds it through `WebServer::AlpacaBackend`, and `tools/alpaca-sim` serves
it on Linux/macOS for ConformU in CI. Discovery is answered by `WebServer::handleAlpacaDiscovery`,
polled once per main-loop pass. Setup URLs redirect to `/settings?section=alpaca` (Safety tab).

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: ESPAsyncWebServer, WiFiUDP, ArduinoJson 6; ConformU 4.5.0 (CI)
**Testing**: Unity (`test_alpaca_logic`, router tests), ConformU workflow
**Target Platform**: ESP32; Linux (simulator)
**Project Type**: Embedded firmware + CI tooling
**Performance Goals**: Discovery reply well under 1 s

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| II. Conformance | Single implementation; ConformU clean in CI | Router shared; workflow runs on PRs |
| III. Testable pure logic | Protocol logic in `lib/` | Router + protocol tested |
| VII. Docs | UI names current | `docs/user-guide/alpaca.md` |

## Project Structure

### Documentation (this feature)

```text
specs/007-ascom-alpaca/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
lib/AlpacaLogic/src/AlpacaRouter.cpp, AlpacaProtocol.cpp, ObservingConditionsMapper.cpp, AlpacaDiscovery.cpp
test/test_alpaca_logic/test_main.cpp
tools/alpaca-sim/main.cpp, tools/alpaca-sim/build.sh
.github/workflows/alpaca-conformance.yml
src/WebServer.cpp            # AlpacaBackend, handleAlpacaRequest, handleAlpacaDiscovery, /setup
web/src/components/Alpaca.tsx
web/src/components/settings/SafetyTab.tsx   # ASCOM Alpaca card ("Serve Alpaca devices")
web/src/components/settings/restart.ts      # "Alpaca discovery" restart reason
docs/user-guide/alpaca.md, docs/reference/troubleshooting.md
```

**Structure Decision**: Shared protocol library + firmware glue + desktop simulator.

## Complexity Tracking

None.
