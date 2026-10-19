# Contract: battery configuration

All additive (FR-016): a config without these keys loads with the defaults and behaves as today, since
every battery behaviour needs a detected gauge. Older firmware ignores them.

## Main config JSON (NVS main key, 5,100 B)

```json
"battery": {
  "enabled": true,
  "lowPercent": 20,
  "criticalPercent": 10,
  "shutdownPercent": 5,
  "capacityMah": 0,
  "powerSaving": true,
  "saveReadIntervalSeconds": 30,
  "shutdownGraceSeconds": 30,
  "alertPin": 255,
  "stat1Pin": 255,
  "stat2Pin": 255,
  "pgPin": 255
},
"alpaca": { "...": "...", "batteryEnabled": false, "batteryMinPercent": 10, "batteryAlert": true, "batteryPhoneAlarm": true },
"mqtt": { "publish": { "...": "...", "battery": true } }
```

| Key | Values | Validation message (exact; device catalogue) |
|---|---|---|
| `battery.lowPercent` | 5..50 | `Battery: low level must be 5 to 50%` |
| `battery.criticalPercent` | 2..25 and < low | `Battery: critical level must be 2 to 25% and below the low level` |
| `battery.shutdownPercent` | 2..10 and < critical | `Battery: shutdown level must be 2 to 10% and below the critical level` |
| `battery.capacityMah` | 0 or 500..50000 | `Battery: capacity must be 500 to 50000 mAh, or 0 if unknown` |
| `battery.saveReadIntervalSeconds` | 10..300 | `Battery: power-saving read interval must be 10 to 300 seconds` |
| `battery.shutdownGraceSeconds` | 10..300 | `Battery: warning time before sleep must be 10 to 300 seconds` |
| `battery.*Pin` | 255 or valid GPIO, distinct | `Battery: pins must be valid and different` |
| `battery.stat1Pin/stat2Pin/pgPin` | all set or all 255 | `Battery: set all three charger pins or none` |
| `battery.*Pin` | not I2C-1/I2C-2, GPS, rain, wind, display button | `Battery: pin is already in use` |
| `battery.pgPin` | RTC-capable when set | `Battery: the USB power pin must be able to wake the device` |
| `alpaca.batteryMinPercent` | 2..50 | `Safety: battery limit must be 2 to 50%` |

The display button check (spec 031) and the wind check include the battery pins.

## Alerts (NVS key `alertsBatt`, new `Config::AlertsPart::Battery`, ≤ 3,900 B)

`alerts.events` (as served by `GET /api/config` and accepted by `POST /api/config`) gains:

| Event key | Default level | Built-in title |
|---|---|---|
| `battery_low` | 2 Normal | `Battery low` |
| `battery_critical` | 4 Wake me | `Battery critical` |
| `battery_shutdown` | 4 Wake me | `Going to sleep: battery empty` |
| `power_restored` | 0 Off | `Power restored` |
| `charge_complete` | 0 Off | `Battery charged` |
| `charge_fault` | 2 Normal | `Charging fault` |

Each has `level`, `sound`, `title`, `message` like every event. The stored `alerts` (Main) and Client
parts are unchanged; `ConfigStore` splices the `alertsBatt` part in on load as it does the Client part.
The existing "too long in total" message covers all three parts.

## Settings dependencies (catalogue additions)

| ID | Class | Settings | Depends on | Reason → text → fix | Unmet |
|---|---|---|---|---|---|
| D-46 | dependency, runtime | `battery.enabled`, `battery.lowPercent`, `battery.criticalPercent`, `battery.shutdownPercent`, `battery.capacityMah`, `battery.powerSaving`, `battery.saveReadIntervalSeconds`, `battery.shutdownGraceSeconds` | gauge detected (`Facts.batteryDetected`) | `battery-missing` → "No battery gauge detected" → `device#battery` | inactive |
| D-47 | dependency, runtime | `alpaca.batteryEnabled` | gauge detected | `battery-missing` | **fail-safe** |
| D-48 | dependency, runtime | `alerts.events.battery_low.level`, `…battery_critical.level`, `…battery_shutdown.level`, `…power_restored.level`, `…charge_complete.level`, `…charge_fault.level` | gauge detected | `battery-missing` | inactive |
| D-49 | dependency | `mqtt.publish.battery` | MQTT on; gauge detected | `mqtt-off`, `battery-missing` | inactive |
| D-50 | dependency | `alpaca.batteryPhoneAlarm` | BLE build, BLE on, passkey, paired phones | `ble-build`, `ble-off`, `no-passkey`, `no-phones` | inactive |
| D-51 | constraint, feature | `battery.alertPin`, `battery.stat1Pin`, `battery.stat2Pin`, `battery.pgPin` | rules above | messages above | — |

`alpaca.batteryMinPercent`, `alpaca.batteryAlert` go in `noDependency` beside their rule (the toggle
a value belongs to doesn't count). Implemented in `lib/SettingsDeps` and `web/src/lib/settingsDeps.ts`,
held together by `test/fixtures/settings-deps/cases.json`; `docs/reference/settings-dependencies.md`
regenerated. Fixtures: `test/fixtures/config-ranges.json`, `test/fixtures/settings-deps/default-config.json`,
schema `config.schema.json`, web Zod schema.
