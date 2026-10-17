# Research: Li-ion battery and fuel gauge

Grounded in the code as of `feature/travel-allsky` (spec 030 implemented, spec 031 planned on the same
branch) and the hardware repo (SQMeter-Hardware `hardware/kicad/README.md`, Rev A travel BOM). Paths are
repo-relative.

## What exists today

- **No battery code** anywhere in `src/`, `include/`, `lib/` or the web UI. No deep sleep: the device
  only restarts (`WebServer::scheduleRestart`). `RTC_DATA_ATTR uint32_t bootCount` in `src/main.cpp` shows
  RTC memory is already used.
- **Hardware (travel board, Rev A)**: ESP32-S3-Zero; MCP73871-2CA power-path charger (USB-C, 833 mA
  charge, **safety timer off**, NTC charge window ~3-44 °C); DW01A + AO3400A cell protection; MAX17048
  fuel gauge at 0x36 on the main I2C bus (GPIO 8/9); gauge ALRT on GPIO 1; charger STAT1/STAT2/PG on GPIO
  2/4/5, open drain, active low. The power switch cuts the system but not charging. The firmware builds
  only for classic `esp32dev` today; S3 board profiles are separate work, so every pin here is
  configurable, `255` = unset, and the defaults touch no pin (GPIO 1 is UART0 TX on the classic board).
- **I2C**: all bus traffic is on the loop task (`setupI2C`, sensor `update()`s, spec 030 non-blocking
  TSL2591 ticks, spec 031 display pages). A gauge read is two 2-byte register reads (~1 ms at 100 kHz).
- **Safety**: `lib/AlpacaLogic` `SafetyThresholds`/`SafetyInputs`/`evaluateSafety` with value-bearing
  reasons via `addReasonf` ("Wind %.1f m/s >= %.1f m/s"); rain and wind are checked regardless of the
  other sensors' freshness; `UnsafeReasonFlag` bits 0-13 are used (bits feed alerts and the BLE alarm).
  `Core::safetyInputs/safetyThresholds` (`lib/DeviceCore`) map config + snapshot to them. Settings live in
  `AlpacaConfig` (`alpaca.*`).
- **Alerts**: `lib/AlertLogic` `AlertEngine` (edge-triggered trackers with cooldown, settle and a 60 s
  startup grace that imaging-app events skip), `AlertType` (16 types, `ALERT_TYPE_COUNT`), levels 0-4
  where **Wake** (4) also rings paired phones (`Core::runAlerts` ORs reason flags into
  `AlertStep::alarmFlags`; `WebServer::sendAlerts` raises the BLE alarm). Events are configured per
  `AlertsConfig::EventSetting` (level, sound, title, message), listed in
  `lib/ConfigModel/src/ConfigInternal.h` `eventSettingsOf` (`EVENT_COUNT` 12, `FIRST_CLIENT_EVENT` 9).
  Stored in NVS under two keys — `alerts` (Main) and the imaging-app part (Client) — each ≤ 3,900 B
  (`MAX_ALERTS_JSON_BYTES`), because templates (80 + 240 chars per event) make them large.
  Template variables: `lib/AlertLogic/template-variables.json` ("common" + per event), mirrored in
  `web/src/components/settings/alertVariables.ts`. Delivery runs on the dispatcher's own task
  (`include/AlertDispatcher.h`, queue of 8, HTTPS one at a time).
- **Readings**: `lib/Readings` writes groups with `status` + values only when ok (never zeros); MQTT
  publish switches `mqtt.publish.*` select groups; HA discovery `ENTITIES[]` table removes an entity
  with an empty payload when its group is off. `DOC_CAPACITY` 4,608 B pool, MQTT payload limit 3,072 B,
  both pinned by `test/test_readings` worst-case tests.
- **BLE**: `WebServer::updateBle` builds the summary JSON in a `StaticJsonDocument<320>`; the alarm
  characteristic carries `reasonFlags` u32 (`lib/BleLogic/BleAlarm.h`).
- **Status / deps / UI / demo**: as in spec 031 research (status writer `src/WebServerStatus.cpp`,
  `Core::writeSensorHealth`/`writeDiagnostics`, `Deps::Facts`, catalogue D-01..D-42 plus 031's
  D-43..D-45, Settings tabs, dashboard `cards.tsx`/`weatherCards.tsx`, glance `web/src/dashboard/glance.ts`,
  inventory `web/src/dashboard/inventory.json`, System `Sensors` card, demo `web/src/demo/*`,
  `tools/demo-core/*`).
- **Budgets**: BLE build 1,690,667 B (92.1%) after 030; the 95% CI fail line is 1,743,257 B. Spec 031
  targets ≤ 9 KB, leaving ~43 KB: this feature targets ≤ 10 KB.

## D1 Own MAX17048 register reads, no library

**Decision**: read the gauge directly with `Wire` in `src/sensors/MAX17048Sensor.cpp`: VCELL (0x02,
78.125 µV/LSB), SOC (0x04, 1/256 %), VERSION (0x08), CRATE (0x16, signed, 0.208 %/h/LSB), STATUS (0x1A,
RI = reset indicator). Detection = ACK at 0x36 and VERSION reading 0x001x. Read every 5 s (FR-002: ≤ 10
s), from the loop task. After a read that sees RI set, clear it (write STATUS) and mark the gauge as just
reset. All decoding (register → value, validity, the estimating window) is pure, in `lib/BatteryLogic`.

**Rationale**: four registers don't justify a library's flash (BLE build headroom); decoding natively
testable (constitution III).

**Alternatives**: SparkFun MAX1704x library (several KB, supports chips we don't have).

## D2 "Estimating" after a gauge reset

**Decision**: percent and time estimates are reported as estimating (null values, `estimating: true`)
for 2 s after the gauge reports RI (fresh power-up) or first detection, and while SOC reads 0 % with
VCELL above 3.5 V (an impossible pair right after power-up). Rate-based estimates need 60 s of samples
after that. Voltage is valid at once.

**Rationale**: edge case "fresh pack" — no misleading 0 % or 100 %; the MAX17048 publishes its first
SOC within ~1 s of power-up (ModelGauge quick-start).

## D3 Charger state from STAT1/STAT2/PG (MCP73871 truth table)

**Decision**: pins read with `INPUT_PULLUP` (open drain, active low; board pull-ups also fitted). Pure
decoding in `lib/BatteryLogic` (`ChargerDecoder`):

| STAT1 | STAT2 | PG | State | usbPower |
|---|---|---|---|---|
| L | H | L | `charging` | true |
| H | L | L | `charged` | true |
| L | L | L | `fault` (reason `temperature`: the safety timer is off on Rev A, so a fault is the NTC window or a bad cell) | true |
| H | H | L | `no-battery` | true |
| H | H | H | `discharging` | false |
| L | H | H | `discharging` (charger's low-battery output) | false |
| other | | | `discharging` if PG high, else `charging` (logged as unexpected) | from PG |

A pattern must hold 2 s before it is reported (MCP73871 status lines can toggle while a pack is absent);
continuous toggling for > 6 s reads as `no-battery`. SC-002 (≤ 5 s) holds. **Charger pins unset**
(classic boards with only a gauge): state from the smoothed gauge rate (> +0.5 %/h `charging`, else
`discharging`), `usbPower` null, `charged`/`fault`/`no-battery` unavailable. The charger's "charged" is
authoritative for "full" (edge case "stuck at 99 %").

**Rationale**: FR-003 from the board as built; one table to change if a later board uses another charger.

## D4 Time estimates

**Decision**: rate = exponential moving average of CRATE with a 10-minute time constant. Time to empty
= percent / |rate| while discharging; time to full = (100 − percent) / rate while charging; both null
when estimating, when |rate| < 0.5 %/h, or in any state other than discharging/charging. The value is
reported in minutes (capped at 5,940 = 99 h; the UI shows "> 99 h" at the cap and "--" when null).
When `battery.capacityMah` is set, time to full is at least the constant-current time at the charger's
833 mA (`(100 − percent)% × capacity / 833 mA`), and status diagnostics show the estimated current
(`rate × capacity`). Pure, natively tested.

**Rationale**: FR-004 and the edge cases (no huge numbers, "--" near zero rate). The gauge gives no
current, so capacity can only refine, not replace, the rate-based estimate.

## D5 Safety rule (constitution I)

**Decision**: `alpaca.batteryEnabled` (false), `alpaca.batteryMinPercent` (default 10, the critical
default; 2..50). Evaluated **independently of the other sensors' freshness**, like rain and wind:

- rule on, gauge absent/estimating/stale (age > `alpaca.staleAfterSeconds`) → `UnsafeBatteryData` (bit
  15) "Battery data missing";
- rule on, percent < limit → `UnsafeBatteryLow` (bit 14) "Battery 8% < 10%";
- shutdown pending (D7), **whether or not the rule is on** → `UnsafeShutdown` (bit 16) "Shutting down:
  battery 4% < 5%" (US3 AC2: unsafe for at least one safety poll before sleeping).

A charging pack below the limit stays unsafe (charge level, not state, is what the rule measures).
Settings dependency: rule enabled with no gauge = fail-safe (unsafe), reason `battery-missing`.

Two rule options: `alpaca.batteryAlert` (true) sends a **Battery rule** alert through the channels at
the Urgent level when bit 14 or 15 newly appears (stacked with the generic unsafe alert of the same
pass, `Alerts::stackAlerts`); `alpaca.batteryPhoneAlarm` (true) raises the BLE alarm with those bits on
the BLE build (FR-008). Neither can change the verdict (constitution I).

## D6 Battery alert events

**Decision**: six new `AlertType`s and `EventSetting`s: `battery_low`, `battery_critical`,
`battery_shutdown` ("going to sleep"), `power_restored`, `charge_complete`, `charge_fault`, plus the
non-configurable `BatteryRule` type (D5). Defaults: critical and shutdown level 4 (Wake), low and
charge fault 2 (Normal), power restored and charge complete 0 (off) (FR-007). Detection in a new
`AlertEngine::updateBattery`:

- low / critical: fire once when percent < threshold (valid data only); re-arm at threshold + 5 %;
- charge complete: state enters `charged`; charge fault: enters `fault` (once until it leaves);
- shutdown and power restored: one-shot inputs from the power policy (D7); **exempt from the startup
  grace** (like imaging-app events), since a boot at low charge or a wake from sleep must not wait 60 s.

Wake-level battery alerts add their bit (14 for low/critical, 16 for shutdown) to `alarmFlags`, so
phones ring through the existing path. Settings dependency: battery events inactive without a gauge.

**Persistence**: the six settings go under a **third NVS key** `alertsBatt` (`Config::AlertsPart::Battery`,
≤ 3,900 B) so neither existing part grows; the API's `alerts.events` shows all events as today. Older
firmware ignores the key. Validation checks all three parts.

## D7 Power policy and deep sleep

**Decision**: `Battery::PowerPolicy` (pure, `lib/BatteryLogic`), ticked with valid gauge data, charger
state and config:

| State | Enter | Actions | Leave |
|---|---|---|---|
| `Normal` | boot; recovery | — | percent < critical, not charging, `powerSaving` on → `Saving` |
| `Saving` | from Normal | (the engine's critical event fires at the same threshold, D6); sensor/snapshot interval → `battery.saveReadIntervalSeconds` (30 s); display forced off (spec 031 `forcedOff`); status `powerSaving: true` | percent ≥ critical + 5 or charging/USB → `Normal` (+ power restored); percent ≤ shutdown on 2 consecutive reads, not charging → `ShutdownPending` |
| `ShutdownPending` | from Saving only (shutdown is part of power saving) | going-to-sleep alert (D6), BLE alarm when enabled, `UnsafeShutdown` in the verdict, Alpaca and REST keep answering | after `battery.shutdownGraceSeconds` (30 s) **and** the dispatcher idle, or grace + 15 s at most → deep sleep; USB/charging during the grace → `Normal` + power restored |

Missing or estimating data never moves the policy towards sleep (a gauge fault must not switch the
device off; the safety rule reports it). Power saving off → no slowing, no display change and no
shutdown (the safety rule and alerts still work).

**Deep sleep**: `esp_sleep_enable_ext0_wakeup(pgPin, 0)` when `battery.pgPin` is set and RTC-capable
(classic: 0, 2, 4, 12-15, 25-27, 32-39; S3: 0-21), plus a timer wake every 60 min as a safety net. RTC
memory keeps `sleptForBattery` and the boot reason. On a timer wake, `setup()` reads the gauge right after
`setupI2C()`; still ≤ shutdown and no USB/charging → back to sleep at once, silently (the alert was
already sent). On a PG wake or charging/USB at a timer wake → normal boot and a **power restored** alert
once alert channels are reachable (WiFi joined) or after 2 minutes. Without a PG pin, the timer wake
alone detects charging (gauge rate) within an hour; the docs say so.

**Rationale**: owner clarification (deep sleep until USB returns, after warning); "doesn't wait
indefinitely" edge case; SC-004 (unsafe seen before the cells cut out: the shutdown level is above the
DW01A's ~2.4 V cut, and unsafe comes first).

**Alternatives**: `esp_deep_sleep` with no timer (a missed PG edge would sleep forever); light sleep
(still drains the cells); WiFi modem sleep in Saving (more saving, but not asked for; noted for later).

## D8 Configuration (additive)

**Decision**: a `battery` section in the main config JSON: `enabled` true, `lowPercent` 20,
`criticalPercent` 10, `shutdownPercent` 5, `capacityMah` 0 (unset), `powerSaving` true,
`saveReadIntervalSeconds` 30, `shutdownGraceSeconds` 30, `alertPin`/`stat1Pin`/`stat2Pin`/`pgPin` 255;
`alpaca.batteryEnabled/batteryMinPercent/batteryAlert/batteryPhoneAlarm`; `mqtt.publish.battery` true.
Everything acts only when a gauge is detected, so old configs and gauge-less boards behave as today
(FR-016). Monitoring off (`battery.enabled` false, FR-005) means the gauge is only probed once at boot (so
Settings can still say "Detected"), the gauge and pins are not read, and
the device reports as if no gauge were fitted (`{"present": false}`, no alerts, no power policy);
the safety rule, if on, then reports "Battery data missing" (fail-safe). About +330 B in the main JSON (pinned < 5,100 B by a test with 031's section and long
strings). The ALRT pin is read for diagnostics only; polling is authoritative.

## D9 What is emitted

**Decision**: one `Readings::Battery` group written by `lib/Readings` for REST, WebSocket and MQTT
state (contracts/readings.md): `{"present": false}` only without a gauge; otherwise status, ageMs and
the FR-011 fields, with null for values that can't be given (estimating, unknown USB, no estimate). MQTT
group `battery` (`mqtt.publish.battery`). `/api/status` adds `battery` (same object) plus
`sensors.battery` health and `diagnostics.battery` (registers, charger pin levels, estimating, policy
state, current estimate). `DOC_CAPACITY` and `STATUS_DOC_CAPACITY` re-checked by their worst-case tests
(+~384 B each expected).

## D10 Home Assistant and BLE

**Decision**: HA entities `battery` (device_class `battery`, `%`), `battery_voltage` (`voltage`, `V`),
`battery_state` (device_class `enum`, options `discharging, charging, charged, fault, no-battery`) and
`battery_time_to_empty` (`duration`, `min`), group `Battery`, published only with a gauge present and
`mqtt.publish.battery` on, else removed (empty payload). BLE summary gains `battery` (integer %) and
`batteryState`; its document grows from 320 to 384 B. Advertisement unchanged.

## D11 Settings dependencies

New reason `battery-missing` ("No battery gauge detected", fix `device#battery`) and
`Facts.batteryDetected`:

| ID | Settings | Depends on | Unmet |
|---|---|---|---|
| D-46 | `battery.*` (except pins) | gauge detected (runtime) | inactive |
| D-47 | `alpaca.batteryEnabled` | gauge detected | **fail-safe** |
| D-48 | `alerts.events.battery_*`, `power_restored`, `charge_complete`, `charge_fault` levels | gauge detected | inactive |
| D-49 | `mqtt.publish.battery` | MQTT on; gauge detected | inactive |
| D-50 | `alpaca.batteryPhoneAlarm` | BLE build, BLE on, passkey, paired phones (as D-08) | inactive |
| D-51 | constraint: battery pins valid, distinct, not used by another bus, PG RTC-capable | — | messages |

Pins are also added to the pin-conflict checks used by wind (spec 030) and the display button (spec 031).

## D12 Web UI

**Decision**: dashboard **Battery** card (Card + MetricTile: percent large, state pill, voltage, time
left; no card without a gauge), a glance tile when charge is below low (warn) or critical / fault (bad);
System → Sensors rows (gauge, version, voltage, charger pins); Settings → Device → **Battery** card
(monitoring toggle, thresholds, capacity, power saving with read interval and grace, pins); Safety tab
battery rule row (toggle, limit, alert and phone-alarm toggles); Alerts tab six event rows in a
"Battery" group. All disabled with reasons via D-46..D-50. Mockup reviewed by the owner first.

## D13 Demo device

**Decision**: the simulator models a 7 Ah pack discharging at ~6 %/h (sped up by the demo clock), with
a demo panel switch "USB power" that turns it to charging at the charger's rate and back; the emulated
device reports `battery.present: true` with the charger pins "set" so every state can be shown; demo
presets include "battery low" and "charge fault". The WASM core compiles `lib/BatteryLogic`.

## D14 Budgets

Expected: flash +8-10 KB per build (gauge reads, decoders, policy, readings/HA/status JSON, alerts), static
RAM < 200 B (+RTC 8 B), UI gzip +3-4 KB, main config +~330 B, `alertsBatt` part ~700 B default (≤ 3,900 B
with every template at its limit in ASCII: 6 × (80 + 240) chars + keys ≈ 2.4 KB; multi-byte text is caught by the existing "too long in total" validation), readings and status
pools re-pinned. Recorded in `specs/032-battery/budgets.md`; deep-sleep current measured on the travel
board when available.
