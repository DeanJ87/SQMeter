# Data Model: Demo That Behaves Like the Device

## Emulated device (in the device core, WebAssembly)

One instance per browser tab. Owns everything the device would decide:

| Part | From | Notes |
|---|---|---|
| Config | `lib/ConfigModel` | Same defaults, JSON and validation as the firmware |
| Sensor snapshot | Simulator inputs → `lib/DeviceCore` | Same `SensorSnapshot` → sky/cloud/dew derivation as the firmware |
| Safety status | `lib/DeviceCore` + `SafetyEvaluator` + `SafeDelayFilter` | Reasons, held/safe-delay, history |
| Alert engine | `lib/AlertLogic` | Levels, cooldowns, darkness rules, templates, stacking; recent alerts list |
| Alerts armed | `lib/DeviceCore` | On/off, "on while N.I.N.A. connected" (the demo's Alpaca connect counts) |
| Alpaca router | `lib/AlpacaLogic` `Router` + DeviceCore backend | Management, SafetyMonitor, ObservingConditions |
| Safety history | `lib/SafetyHistoryLogic` | Boots are the demo's (re)starts |
| System info | Demo-supplied `SystemInfo` | Plausible heap/partitions/WiFi; uptime from demo start |

State transitions: `applyConfig(json)` → validated or rejected with the device's message; restart-
required changes are queued until `restart()`; `tick(now, inputs)` advances readings, safety, alerts.

## Simulator inputs (TypeScript)

| Field | Unit | Source |
|---|---|---|
| lux, visible/IR/full counts | lux, counts | Sun altitude (core) + moon + scenario |
| sky / ambient IR temperature | °C | Scenario cloud amount |
| temperature, humidity, pressure | °C, %, hPa | Smooth diurnal curve + scenario |
| rain rate, accumulation | mm/h, mm | Rain scenario |
| wind speed, gust, direction | m/s, ° | Scenario |
| GPS fix, position | - | Config location when GPS is on |
| sensor present / failed | flags | Config (enabled) + failure scenario |

## Scenario

`{ id: 'rain' | 'cloud' | 'clear' | 'fail-light' | 'fail-ir' | 'fail-environment' | 'fail-rain' | 'dawn',
startedAt, durationS }` - one active at a time; ends back to the baseline sky. Demo time multiplier
(1× or 10×) shortens device delays (rain clear delay, safe delay, cooldowns); shown in the panel.

## Persisted demo state (sessionStorage, key `sqm.demo.v1`)

`{ version: 1, config, armed, safetyHistory, alerts, scenario, timeMultiplier, savedAt }`. Unknown
version or corrupt data → start from defaults. "Reset demo" deletes the key and reloads.
