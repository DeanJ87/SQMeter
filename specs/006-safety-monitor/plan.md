# Implementation Plan: Safety Monitor

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-safety-monitor/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`evaluateSafety` (lib/AlpacaLogic, natively tested) evaluates inputs built from the sensor snapshot
against thresholds from `alpaca` config; `SafeDelayFilter` holds "safe". `WebServer::updateSafetyStatus`
runs every second, records changes in `SafetyHistory` (RTC memory) and feeds alerts. The verdict is
served via Alpaca, REST, WebSocket and MQTT; the dashboard Safety card shows it with History.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Storage**: NVS (`alpaca` thresholds); RTC_NOINIT memory (history)
**Testing**: Unity (`test_alpaca_logic`), Vitest (SafetyCard, SafetyHistory)
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI
**Performance Goals**: Evaluate every 1 s

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe | Rain independent; missing/stale/unmeasurable = unsafe; values in reasons | Implemented and tested |
| III. Testable pure logic | Evaluator, delay filter and history logic in `lib/` with tests | Evaluator + filter yes; `src/SafetyHistory.cpp` (ring buffer, lastAlert) untested |
| VII. Docs | Rules named as in the UI | `docs/user-guide/alpaca.md` → Safety rules |

## Project Structure

### Documentation (this feature)

```text
specs/006-safety-monitor/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
lib/AlpacaLogic/src/SafetyEvaluator.cpp   # rules, reasons with values, SafeDelayFilter
test/test_alpaca_logic/test_main.cpp
src/WebServer.cpp                         # buildAlpacaSafetyInputs/Thresholds, updateSafetyStatus,
                                          # /api/safety, /api/safe, /api/safety/history, MQTT safety
src/SafetyHistory.cpp, include/SafetyHistory.h
src/Config.cpp                            # alpaca thresholds
web/src/components/SafetyCard.tsx         # verdict, reasons, History
web/src/components/settings/SafetyTab.tsx # rules
docs/user-guide/alpaca.md                 # Safety rules, Safe delay
docs/api/rest.md, docs/user-guide/mqtt.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
