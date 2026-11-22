# Implementation Plan: Rain needs cloud

**Branch**: `036-rain-needs-cloud` | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

## Summary

An owner opt-in rule, **Rain needs cloud**, lets a fresh, healthy clear-sky reading from the IR sky
sensor overrule the rain sensor: rain under a sky below the corroboration level (default 10%) is
reported as **dew suspected** instead of unsafe. The decision is a pure, stateful classifier in
`lib/AlpacaLogic` (`RainTrust`): it classes each evaluation's rain as dry, trusted (setting off,
cloud at or above the level, no usable sky reading, or latched) or dew suspected, and latches trust
until the rain sensor's own hold-off ends. `evaluateSafety` reads the classification; the firmware
and the demo hold one latch next to their safe-delay filter. The dew-suspected state travels with
the safety status to `/api/safety`, MQTT `<base>/safety` (plus a Home Assistant binary sensor), the
dashboard rain card and an optional `dew_suspected` alert. Two settings are added
(`alpaca.rainNeedsCloud`, `alpaca.rainNeedsCloudPercent`), with a settings dependency (D-52) on the
rain sensor and the IR sky sensor.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5), TypeScript (strict), Python 3 tools

**Primary Dependencies**: ArduinoJson 6, Preact + Vite, Unity (native tests), Vitest, Emscripten
(demo core)

**Storage**: NVS main config JSON (two new keys in `alpaca`), alerts JSON (one new event, stored
with the third alerts key)

**Testing**: `pio test -e native` (new `test_rain_trust`, plus additions to
`test_alpaca_safety`, `test_device_core`, `test_alert_logic`, `test_config_model`,
`test_readings`, `test_settings_deps`), Vitest, the i18n, quality, API and docs checks

**Target Platform**: ESP32 / ESP32-S3 firmware, the web UI, and the WebAssembly demo core

**Project Type**: embedded firmware + web UI

**Performance Goals**: one extra classification per 1 s safety pass; no allocation in the hot path

**Constraints**: main config JSON within 5100 bytes; safety document within `SAFETY_CAPACITY`;
gzipped UI growth well under 10 KB (SIZE-01); both firmware images within their slots (SIZE-04)

**Scale/Scope**: one rule, one flag, one alert event, one HA entity, ~10 UI strings in 14 languages

## Constitution Check

*GATE: checked before research and again after design.*

| Principle | How this plan meets it | Result |
|---|---|---|
| I. Fail-safe verdict | Rain is still read regardless of the other sensors' freshness. Only a clear reading from an IR sky sensor that is detected, OK, fresh and computable can overrule it, and only when the owner opts in. Every other case (sensor missing, faulted, stale, uncomputable) trusts rain. Trusted rain stays trusted until the rain sensor's hold-off ends (latch). Reasons state rain intensity, cloud and level. Alerts only read the flag. | Pass with amendment (see Complexity Tracking) |
| II. Alpaca conformance | `IsSafe` semantics unchanged; no new Alpaca surface | Pass |
| III. Testable pure logic | `RainTrust` and the reason wording live in `lib/AlpacaLogic`, covered by native tests including the spec's replay cases; UI covered by Vitest | Pass |
| IV. Budgets | No new task or buffer; config grows ~55 bytes; safety document grows ~90 bytes inside the existing capacity; sizes re-measured | Pass |
| V. Quiet UI | One more rule row in the existing Rain group (DS-01), explanation behind "?" (DS-04), dependency disables it with a reason and a fix link, one note line in the existing warning list of the rain card | Pass |
| VI. Security | `/api/config` writes are already behind `requireAuth`; nothing new is secret | Pass |
| VII. Docs | User guide (safety, alerts, dashboard, MQTT), REST reference, OpenAPI, settings reference, CHANGELOG | Pass |
| VIII. Quality | Coding standard followed; `check.py`, i18n and copy checks run before the PR | Pass |

## Project Structure

### Documentation (this feature)

```text
specs/036-rain-needs-cloud/
├── spec.md
├── plan.md            # this file
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── interfaces.md  # config keys, /api/safety, MQTT/HA, alert event
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
lib/AlpacaLogic/include/RainTrust.h        # classification + latch (new)
lib/AlpacaLogic/src/RainTrust.cpp
lib/AlpacaLogic/include/SafetyEvaluator.h  # inputs/thresholds/result fields
lib/AlpacaLogic/src/SafetyEvaluator.cpp    # checkRain wording, dew flag
lib/ConfigModel/                           # alpaca.rainNeedsCloud(+Percent), alerts dew_suspected
lib/DeviceCore/                            # inputs from the snapshot, status, /api/safety, alerts
lib/AlertLogic/                            # DewSuspected alert
lib/Readings/src/Readings.cpp              # HA binary sensor dew_suspected
lib/SettingsDeps/ (+ catalogue.json)       # D-52
src/WebServerAlerts.cpp, include/WebServer.h  # firmware owns a RainTrust latch
tools/demo-core/                           # the demo owns one too; core rebuilt
web/src/components/settings/SafetyTab.tsx  # rule row
web/src/components/dashboard/weatherCards.tsx  # dew note
web/src/{types,lib,mocks,i18n}/ ...
test/test_rain_trust/                      # new native tests
docs/ , CHANGELOG.md, .specify/memory/constitution.md
```

**Structure Decision**: the existing firmware + `lib/` + web layout; the rule is one new pure unit in
`lib/AlpacaLogic` next to `SafetyEvaluator`, so the firmware, the demo and the native tests share it
(STRUCT-02, STRUCT-07).

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Principle I: "Rain MUST be evaluated independently of every other sensor's freshness". With Rain needs cloud on, a healthy sky reading can change the rain outcome. | A dewed optical rain gauge parks the telescope all night under a clear sky (three nights in November 2026); the owner currently toggles the rain rule by hand, which is less safe than a rule. | Leaving the rule as it is keeps the false parks and the manual toggling. A delay on rain would delay real rain too. Using the cloud-cover safety limit would tie rain to a limit set for a different purpose (often 90%). |

The amendment (MINOR, 1.3.1 → 1.4.0) keeps the guarantee that a stale, missing or faulted sky
sensor can never hide rain, and adds the one exception: an owner opt-in corroboration rule, under
which rain is always trusted unless the sky sensor is enabled, fresh, healthy and its cloud cover
computable and below the level, and trusted rain stays trusted until the rain sensor's hold-off
ends.

Post-design re-check: the design adds no further violations.
