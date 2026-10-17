# Contract: web UI

DS rules relied on: DS-01 (cards from shared parts), DS-02 (state as one pill), DS-03 (MetricTile /
ReadingRow), DS-04 (reasons behind "?"), DS-06/07, DS-08 (one place per fact), DS-09 (phone width),
DS-20..25 (copy), DS-27 (one name: "Battery"). Constitution V: no battery card, tile, row or setting
enabled on boards without a gauge. A mockup of every state below (390 px and 1280 px) goes in
`specs/032-battery/mockups/` and is reviewed by the owner before the components are built.

## Dashboard (FR-014)

**Battery card** (only with `battery.present`), `Card` icon battery:

| State | Shows |
|---|---|
| discharging | large `82%`, pill "Discharging", `MetricTile`s Voltage `3.91 V`, Time left `13 h 17 min` |
| charging | pill "Charging" (cyan), Time to full |
| charged | pill "Charged" (green), no time tile |
| fault | pill "Charging fault" (red), "?" with "Too cold or too hot to charge" |
| no-battery | pill "No battery", no percent |
| estimating | percent "Estimating", time tiles "--" |
| power saving | small `Pill` "Power saving" beside the state pill |
| status not ok | the existing unavailable/fault card pattern titled "Battery" |

Time values: "--" when null, "> 99 h" at the cap. **Glance**: a tile only when percent < low (warn),
< critical or state fault (bad), shutdown pending (bad, "Going to sleep"); none otherwise.
Inventory: every `readings.battery.*`, `status.battery.*`, `status.sensors.battery`,
`status.diagnostics.battery.*` and D-46..D-51 mapped (shown with a test, or not shown with reason).

## System page

Sensors card (`system.sensors`): row "Battery gauge" (Detected / Not detected / status), version,
voltage, charger pins state; only with a gauge or when charger pins are set.

## Settings → Device → Battery card (`#battery`, FR-015)

`StatusBadge` Detected / Not detected; "Monitor battery" `DepToggle`; "Low level" / "Critical level" /
"Shutdown level" `NumberInput` (%) with "?" on what each does; "Pack capacity" (mAh, empty = unknown);
"Save power when low" `DepToggle` with "Read sensors every" (s) and "Warn before sleeping" (s);
"Pins" group: "Gauge alert", "Charger STAT1", "Charger STAT2", "USB power (PG)" (empty = not wired).
Everything disabled with "No battery gauge detected" (D-46) except the pins.

## Settings → Safety

Battery rule row in the existing rules list: "Unsafe below" `DepToggle` (D-47 fail-safe note) +
`NumberInput` %; "Send an alert" `Toggle` (`alpaca.batteryAlert`); "Ring paired phones" `DepToggle`
(D-50).

## Settings → Alerts

The six events as `AlertEventRow`s under a "Battery" group heading in `AlertsEventsCard`, with Test,
level, sound and template editor like every event; disabled with D-48 without a gauge.

## Strings

All labels, pills, notes, options and errors in `web/src/i18n/en.json` with context notes and the 13
other locales (FR-018). Device-built messages (validation, reasons, deps text, alert titles) via the
device catalogue.
