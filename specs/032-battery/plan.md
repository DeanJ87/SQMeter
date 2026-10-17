# Implementation Plan: Li-ion battery and fuel gauge

**Branch**: `feature/travel-allsky` (spec branch `spec/032-battery`) | **Date**: 2026-10-17 |
**Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

## Summary

Read the travel board's MAX17048 fuel gauge (0x36, main I2C bus) with direct register reads and the
MCP73871 charger's STAT1/STAT2/PG lines, and decide everything in a new pure library,
`lib/BatteryLogic`: gauge decoding with an "estimating" window, the charger truth table with
debounce, time estimates, and a power policy (Normal → Saving → ShutdownPending → deep sleep). The
results join the shared `SensorSnapshot`, so one `battery` object reaches REST, WebSocket, MQTT,
`/api/status`, Home Assistant, BLE, alert variables, the OLED Battery page (spec 031) and the
dashboard. A new optional SafetyMonitor battery rule is fail-safe (missing data → unsafe) and a
pending shutdown always reports unsafe before the device sleeps; six battery alert events, stored under
a third NVS key, use the existing channels, levels and the Wake-me phone alarm. Every behaviour needs a
detected gauge, so existing boards and configs are unchanged.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5, pioarduino); TypeScript strict (Preact + Vite); Python 3 tools

**Primary Dependencies**: Arduino `Wire`, GPIO and `esp_sleep` (no gauge library, research D1);
ArduinoJson 6; PubSubClient; NimBLE (BLE build); Zod

**Storage**: NVS main config JSON (5,100 B; +~330 B), new NVS key `alertsBatt` (≤ 3,900 B), RTC memory
(sleep flags, 8 B)

**Testing**: Unity native tests (new `test/test_battery_logic`; `test_alpaca_safety`,
`test_alert_logic`, `test_readings`, `test_device_core`, ...), Vitest + MSW, contract schemas, OpenAPI
check, settings-deps cases, ConformU (safety evaluator changes)

**Target Platform**: travel board (ESP32-S3, Rev A) once its board profile lands; classic ESP32 with a
gauge on the I2C bus (charger pins optional); browser demo (WASM core)

**Project Type**: embedded firmware + embedded web UI + docs

**Performance Goals**: battery values refreshed every 5 s (FR-002 ≤ 10 s); charging state change
reported within 5 s (SC-002); gauge read ~1 ms on the loop task

**Constraints**: OTA slot (SIZE-04; BLE build ~43 KB to the 95% line after spec 031: target ≤ 10 KB),
loop stack headroom > 2 KB, UI gzip growth < 10 KB, main config < 5,100 B, each alerts part ≤ 3,900 B,
readings doc within `DOC_CAPACITY` and 3,072 B MQTT, no change without a gauge (SC-005), never sleep on
missing data

**Scale/Scope**: 1 gauge + 1 charger; ~18 firmware/lib files, ~14 web files, 10 docs pages

## Constitution Check

*Gate before Phase 0; re-checked after Phase 1 (below).*

| Principle | How this plan satisfies it | Status |
|---|---|---|
| I Fail-safe verdict | Battery rule: missing/estimating/stale data → unsafe with "Battery data missing"; low → "Battery 8% < 10%" (value + limit); evaluated independently of other sensors' freshness; pending shutdown always unsafe for ≥ the grace; alerts and phone alarms can't change the verdict; rule enabled without a gauge is fail-safe (D-47) | Pass |
| II Alpaca conformance | Same `Alpaca::Router`; only the evaluator gains rules; ConformU in CI with zero errors/issues | Pass |
| III Testable pure logic | Gauge/charger decoding, estimates, power policy in `lib/BatteryLogic`; safety in `lib/AlpacaLogic`; alerts in `lib/AlertLogic`; readings/HA in `lib/Readings`; config/deps in their libs; all native-tested; UI in Vitest | Pass |
| IV Resource budgets | No library; reads on the loop task (~1 ms); third NVS key keeps alert parts under 3,900 B; document pools re-pinned; sizes, heap, stack measured into budgets.md | Pass |
| V Quiet UI | Shared components only; no battery card/tile/settings enabled without a gauge; reasons via settings deps; mockup reviewed first | Pass (mockup review is a task gate) |
| VI Trusted-LAN security | No new routes; settings through the authenticated config API; no secrets | Pass |
| VII Docs with behaviour | New `docs/user-guide/battery.md`; travel board on hardware pages; configuration, REST, WebSocket, MQTT, BLE, alerts, Alpaca, settings-dependencies in the same change (FR-017) | Pass |
| VIII Code quality | `tools/quality/check.py` incl. DASH-01/02, DS, deps, i18n; no baseline growth | Pass |
| Compatibility | Additive config and documents; `battery: {present: false}` is the only new field on existing boards (FR-011 AC3); older firmware ignores `alertsBatt` | Pass |

## Project Structure

### Documentation (this feature)

```text
specs/032-battery/
├── spec.md
├── plan.md              # this file
├── research.md          # D1-D14
├── data-model.md
├── quickstart.md
├── budgets.md           # filled during implementation
├── contracts/
│   ├── config.md
│   ├── readings.md      # readings + /api/status
│   ├── integrations.md  # safety, alerts, HA, BLE
│   └── ui.md
├── mockups/             # owner review before UI build
└── tasks.md
```

### Source Code (repository root)

```text
lib/BatteryLogic/include/BatteryLogic.h       GaugeDecoder, ChargerDecoder, Estimate, PowerPolicy, ChargeState names
lib/BatteryLogic/src/*.cpp                    (no Arduino headers)
lib/SensorTypes/include/SensorTypes.h         BatteryReading (raw registers + decoded values)
lib/ConfigModel/                              BatteryConfig, alpaca.battery*, mqtt.publish.battery, six EventSettings,
                                              AlertsPart::Battery, validation, pin checks (with wind and display button)
lib/DeviceCore/                               BatterySnapshot in SensorSnapshot, readings/status/diagnostics, safety inputs,
                                              alert inputs/vars/samples, deps facts, display view battery group (spec 031)
lib/Readings/                                 Battery group, HA entities, DOC_CAPACITY
lib/AlpacaLogic/                              battery thresholds/inputs, reasons, flags 14-16
lib/AlertLogic/                               AlertTypes, updateBattery, template-variables.json
lib/SettingsDeps/ + catalogue.json            D-46..D-51, reason battery-missing
include/sensors/MAX17048Sensor.h, src/sensors/MAX17048Sensor.cpp   gauge reads, charger pins
include/PowerManager.h, src/PowerManager.cpp  applies the policy: read interval, display forcedOff, sleep (ext0 + timer), RTC flags
src/ConfigStore.cpp                           alertsBatt NVS key
src/main.cpp                                  gauge + power manager wiring, early sleep check after setupI2C
include/WebServer.h, src/WebServer*.cpp       snapshot copy, status, BLE summary, alert/rule alarm, dispatcher-idle query
include/AlertDispatcher.h, src/AlertDispatcher.cpp   idle() for the shutdown grace
platformio.ini, tools/demo-core/*             -Ilib/BatteryLogic/include; WASM sources; demo battery feed
tools/api/operations.json, docs/api/openapi.json
specs/016-demo-device-emulation/contracts/schemas/{readings,status,config,settings-effective}.schema.json
test/test_battery_logic/, test_config_model, test_config_migration, test_device_core, test_readings,
test/test_alpaca_safety, test_alert_logic, test_alert_variables, test_settings_deps, test_ble_logic, fixtures
web/src/types, validation, components/dashboard/(BatteryCard in cards.tsx or batteryCard.tsx), dashboard/glance.ts, inventory.json,
web/src/components/settings/{BatteryCard.tsx,DeviceTab.tsx,SafetyTab.tsx,AlertsEventsCard.tsx,alertVariables.ts,defaults.ts,payload.ts},
web/src/components/System.tsx, lib/settingsDeps.ts, mocks/, demo/, i18n/
docs/user-guide/battery.md (new), docs/hardware/overview.md, docs/getting-started/hardware.md,
docs/user-guide/{configuration,mqtt,ble,alerts,alpaca}.md, docs/api/{rest,websocket}.md,
docs/reference/settings-dependencies.md, mkdocs.yml
```

**Structure Decision**: the existing layout plus one new library, `lib/BatteryLogic`; firmware glue is
a gauge driver and a power manager.

## Phases

1. **Foundation**: `lib/BatteryLogic` types, config model + validation + migration + alerts part,
   settings deps (tests first).
2. **US1**: gauge/charger decoding, estimates, snapshot, readings, status; driver and wiring; dashboard
   card and glance after the mockup review.
3. **US2**: safety rule and reasons, battery alert events, rule alert and phone alarm.
4. **US3**: power policy, power manager (interval, display off, sleep and wake), going-to-sleep and
   power-restored alerts.
5. **US4**: HA entities, template variables, BLE summary.
6. **Demo, contracts, strings, docs, budgets, gates**.

## Post-design Constitution re-check

The Phase 1 artifacts keep every gate: documents and config are additive; the safety rule's unmet
behaviour is fail-safe and its reasons carry value and limit; the policy never sleeps on missing data;
the UI contract uses only shared components and settings dependencies. No violations, so no
Complexity Tracking entries.

## Complexity Tracking

None.
