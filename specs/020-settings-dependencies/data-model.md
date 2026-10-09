# Data Model: Settings Dependencies

## Catalogue entry (`lib/SettingsDeps/catalogue.json`)

| Field | Type | Rule |
|---|---|---|
| `id` | string `D-NN` | Unique, stable; never reused. |
| `class` | `dependency` \| `constraint` | Never both (FR-010). |
| `kind` | `setting` \| `feature` | `setting`: reported by `GET /api/settings/effective`. `feature`: behaviour already handled outside the report (Alpaca properties, the Sun & Moon card) - tested, not reported. Constraints are always `feature`. |
| `settings` | string[] | Config JSON paths (or virtual paths, e.g. `alerts.wakePhones`) the entry governs. |
| `dependsOn` | string | Human description. |
| `reasons` | `{code, text, fix}[]` | Reason codes this entry can produce; `fix` is `tab#anchor` or `restart`. |
| `unmet` | `inactive` \| `fail-safe` | Safety rules only (FR-009). |
| `runtime` | bool | Depends on a runtime fact (WiFi, broker, clock, GPS fix, detection) rather than a setting: never blocks a toggle. |
| `neutral` | bool | Inactive state shown as a muted note, not a warning (SC-005, D-14). |

## Facts (device → report)

| Field | Meaning |
|---|---|
| `wifiConnected` | Joined a network (not the setup hotspot). |
| `mqttConnected` | Broker connection up. |
| `clockSet` | Clock past 2024-01-01 (NTP or GPS time). |
| `gpsRunning` | GPS started at boot (it only starts at boot). |
| `gpsFix` | GPS has a fix. |
| `bluetoothBuild` | Firmware built with Bluetooth. |
| `bluetoothRunning` | Bluetooth started at boot (it only starts at boot). |
| `pairedPhones` | Bonded phones. |
| `lightDetected` / `infraredDetected` / `environmentDetected` | TSL2591 / MLX90614 / BME280 found and responding (status ok or stale). |

Rain and wind apply live when saved, so they need no "running" fact.

## Effective state (per reported setting)

| Field | Type | Rule |
|---|---|---|
| `id` | string | Catalogue ID of the deciding link (the first unmet link, or the entry's own ID when active/off). |
| `setting` | string | Config JSON path. |
| `state` | `off` \| `active` \| `inactive` | Device never reports `unknown`; the UI uses `unknown` before the report loads. |
| `reason`, `text`, `fix` | string | Present only when `inactive`. `text` is the human reason ("MQTT is off"). |
| `unmet` | `inactive` \| `fail-safe` | Safety rules only. |

**Transitions**: `off` ↔ (`active` | `inactive`) by the setting's own switch; `active` ↔ `inactive` by the dependency (config, restart or runtime fact). The dependent's stored value never changes (FR-001, FR-008).

**Chain order**: links are evaluated in the order listed for the setting; the first unmet link decides `id`, `reason`, `fix`. A link met in two ways (location: settings or GPS fix) is unmet only when neither holds.

## Safety document addition

`rulesNotInEffect: string[]` - rules switched on whose unmet behaviour is `inactive` and whose dependency is unmet, e.g. `"Unsafe while raining - rain sensor is off"`. Always present (possibly empty).

## Alert delivery record

A channel switched on but inactive is recorded `skipped` with the reason text; no delivery is attempted.
