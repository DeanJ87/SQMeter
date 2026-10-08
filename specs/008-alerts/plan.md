# Implementation Plan: Alerts

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/008-alerts/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`AlertEngine` (lib/AlertLogic, tested) turns inputs into edge-triggered alerts with cooldown,
settle times, darkness limits, seeding and stacking, plus `renderTemplate`. `WebServer::processAlerts`
applies per-event levels/sounds/templates and the on/off switch, then `AlertDispatcher` delivers via
MQTT (loop task) and Pushover/ntfy/webhook (own task, one TLS session), recording per-channel
results. The Alerts tab edits events, wording and channels; the bell shows recent alerts.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: WiFiClientSecure (pinned root CAs), PubSubClient, Preferences
**Storage**: NVS (`alerts` key ≤ 3900 bytes; `sqm-alerts/armed`)
**Testing**: Unity (`test_alert_logic`), Vitest (AlertLevels, AlertsTestDelivery, AlertsBell, SkyAlerts)
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| III. Testable pure logic | Engine, templates, stacking tested | Yes; level→channel priority mapping lives in AlertDispatcher, untested |
| IV. Budgets | One TLS session; alerts JSON ≤ 3900 | TlsLock; validated |
| VI. Security | Secrets masked; actions need auth | Yes |
| VII. Docs | UI labels match | docs/user-guide/alerts.md |

## Project Structure

### Documentation (this feature)

```text
specs/008-alerts/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
lib/AlertLogic/src/AlertEngine.cpp, lib/AlertLogic/include/AlertEngine.h
test/test_alert_logic/test_main.cpp
src/AlertDispatcher.cpp            # channels, priorities, retries, records
src/WebServer.cpp                  # processAlerts, alertVars, templates, tests, arm/disarm, routes
src/Config.cpp                     # alerts config, migration, validation
web/src/components/settings/AlertsTab.tsx   # events, wording editor (DEFAULT_TEXT, COMMON_VARS), channels
web/src/components/AlertsBell.tsx
docs/user-guide/alerts.md, docs/api/rest.md, docs/user-guide/mqtt.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
