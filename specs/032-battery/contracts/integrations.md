# Contract: safety, alerts, Home Assistant, BLE

## SafetyMonitor (FR-008, constitution I)

Evaluated regardless of the other sensors' freshness (like rain and wind). Reasons are literal and carry
value and limit:

| Condition | Flag | Reason |
|---|---|---|
| rule on, no gauge / estimating / stale (age > `alpaca.staleAfterSeconds`) | `UnsafeBatteryData` (bit 15) | `Battery data missing` |
| rule on, percent < `batteryMinPercent` | `UnsafeBatteryLow` (bit 14) | `Battery 8% < 10%` |
| shutdown pending (rule on or off) | `UnsafeShutdown` (bit 16) | `Shutting down: battery 4% <= 5%` |

Percent is printed as an integer (floor). `Deps::rulesNotInEffect` doesn't list the battery rule: its
unmet behaviour is fail-safe. ConformU must pass unchanged (IsSafe still a boolean from the same
evaluator).

## Alerts

- Events (contracts/config.md) detected by `AlertEngine::updateBattery`: low and critical once per
  crossing below the threshold, re-armed at threshold + 5 %; charge complete on entering `charged`;
  charge fault on entering `fault`; going to sleep and power restored from the power policy, exempt from
  the startup grace and the cooldown (one-shot by construction). The other battery events use the
  cooldown as every event does.
- **Battery rule alert** (`AlertType::BatteryRule`, title `Battery: unsafe`, message = the battery
  reason): sent at Urgent when `alpaca.batteryAlert` is on and bit 14 or 15 newly appears in the
  verdict; stacked with the generic unsafe alert of the same pass. With `alpaca.batteryPhoneAlarm` on
  (BLE build, passkey set) the BLE alarm is raised with that bit regardless of event levels (still subject to the alert schedule, like every phone alarm).
- Wake-level battery events add their flag to `AlertStep::alarmFlags` (low/critical → bit 14, going
  to sleep → bit 16), so paired phones ring.
- Template variables, added to "common" in `lib/AlertLogic/template-variables.json` and
  `web/src/components/settings/alertVariables.ts`:

| Variable | Example | Without a gauge / no value |
|---|---|---|
| `{battery}` | `82%` | `--` |
| `{battery_voltage}` | `3.91 V` | `--` |
| `{battery_state}` | `discharging` / `charging` / `charged` / `fault` / `no battery` | `--` |
| `{battery_time_left}` | `13 h 17 min` (to empty while discharging, to full while charging) | `--` |

- Sample alerts for the six events in `Core::sampleAlert` (Alerts settings "Test").
- MQTT `<base>/alerts` carries them like every alert; `docs/user-guide/alerts.md` lists them.

## Home Assistant discovery (FR-012)

Published only while a gauge is present and `mqtt.publish.battery` is on; otherwise each config topic
gets an empty payload (removal):

| object_id | Name | device_class | unit | value_template |
|---|---|---|---|---|
| `battery` | Battery | `battery` | `%` | `value_json.battery.percent` |
| `battery_voltage` | Battery voltage | `voltage` | `V` | `value_json.battery.voltage` |
| `battery_state` | Charging state | `enum` (options `discharging`, `charging`, `charged`, `fault`, `no-battery`) | — | `value_json.battery.state` |
| `battery_time_to_empty` | Battery time to empty | `duration` | `min` | `value_json.battery.timeToEmptyMinutes` |

`Readings::Groups.battery` = gauge present && `mqtt.publish.battery`. Discovery republishes when
presence changes (like the per-sky-sensor entities of spec 030).

## BLE

Summary JSON adds `battery` (integer %, only when valid) and `batteryState` (state string, with a gauge);
document 384 B. Alarm `reasonFlags` may carry bits 14-16 (documented in `docs/user-guide/ble.md`).
Advertisement unchanged.
