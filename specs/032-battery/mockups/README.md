# Mockups: battery

Every state in [contracts/ui.md](../contracts/ui.md). Wireframes for now (390 px phone width shown;
at 1280 px each card takes its usual column in the existing grid, same rows). Rendered
`-phone.png` / `-desktop.png` captures drawn with the app's own markup and CSS are still to be
added. Only existing parts are used: `Card`, `MetricTile`, `Pill`, `ReadingRow`, `SettingsCard`,
`StatusBadge`, `Toggle`, `DepToggle`, `NumberInput`, `AlertEventRow`, `Note`, "?" tips. Nothing
here appears on a board without a fuel gauge (constitution V).

## Dashboard → Battery card (only with `battery.present`)

### b1-discharging

```
┌ [battery] Battery ─────────── (Discharging) ┐
│                                              │
│   82%                                        │
│                                              │
│ ┌ Voltage ──────┐ ┌ Time left ─────────────┐ │
│ │ 3.91 V        │ │ 13 h 17 min            │ │
│ └───────────────┘ └────────────────────────┘ │
└──────────────────────────────────────────────┘
```

### b2-charging

Pill "Charging" (cyan). Second tile "Time to full" `3 h 05 min`; "--" while null; "> 99 h" at the
5,940-minute cap.

### b3-charged

Pill "Charged" (green). Percent `100%`, Voltage tile only (no time tile).

### b4-fault

Pill "Charging fault" (red) with "?" beside it: "Too cold or too hot to charge". Percent and
Voltage as discharging; no time tile.

### b5-no-battery

Pill "No battery". No percent; Voltage tile only when the gauge still answers.

### b6-estimating

Percent reads "Estimating"; time tile "--".

### b7-power-saving

A small second `Pill` "Power saving" beside the state pill (b1 layout otherwise).

### b8-unavailable (status not ok)

The existing unavailable/fault card pattern titled "Battery" (as the Wind card when the anemometer
stops answering), with the charger's pill when the charger pins are wired (e.g. "No battery" on USB
with the pack removed).

## Dashboard → glance row

- below the low level: warn tile "Battery 18%";
- below the critical level or charging fault: bad tile "Battery 8%" / "Charging fault";
- shutdown pending: bad tile "Going to sleep";
- otherwise, or without a gauge: no tile.

## System → Sensors card (`system.sensors`)

```
│ Battery gauge          Detected · v0x0012    │
│ Battery voltage        3.91 V                │
│ Charger pins           STAT1 H · STAT2 H · PG H │
```

Only with a gauge or with the charger pins set. "Not detected" / the status word otherwise.

## Settings → Device → Battery card (`#battery`, after Display)

### s1-detected

```
┌ Battery ──────────────────────── [Detected] ┐
│ Monitor battery                        (on)  │
│ Low level          [ 20 ] %   ?              │
│ Critical level     [ 10 ] %   ?              │
│ Shutdown level     [  5 ] %   ?              │
│ Pack capacity      [      ] mAh (empty = unknown) │
│ Save power when low                    (on)  │
│   Read sensors every   [ 30 ] s              │
│   Warn before sleeping [ 30 ] s              │
│ Pins                                         │
│   Gauge alert      [      ]  (empty = not wired) │
│   Charger STAT1    [      ]                  │
│   Charger STAT2    [      ]                  │
│   USB power (PG)   [      ]                  │
└──────────────────────────────────────────────┘
```

"?" texts: low sends the Battery low alert; critical starts power saving and the critical alert;
shutdown warns, then sleeps until USB power returns.

### s2-not-detected

Badge "Not detected" (dim). Every row but the pins disabled with the D-46 note "No battery gauge
detected" (muted, no warning colour: monitoring ships on).

### s3-validation

Inline errors with the device's messages, e.g. under Critical level "2 to 25%, below the low
level"; under a pin "Battery: pin is already in use" (D-51).

## Settings → Safety → battery rule row

```
│ Unsafe below       (off)  [ 10 ] %           │
│   Send an alert                        (on)  │
│   Ring paired phones                   (on)  │
```

Without a gauge the toggle shows the D-47 fail-safe note "Reports unsafe: No battery gauge
detected". "Ring paired phones" shows D-50 (Bluetooth build, passkey, paired phone) muted.

## Settings → Alerts → Battery group

A "Battery" heading in the events card with six `AlertEventRow`s: Battery low, Battery critical,
Going to sleep: battery empty, Power restored, Battery charged, Charging fault. Each with Test,
level, sound and the template editor like every event; disabled with D-48 (muted) without a gauge.

## Review

Pending. The dashboard card, glance tile, System rows, Settings card, Safety row and Alerts group
(T028-T030, T040, T041, T054) were built from these wireframes in round 2, as specs 030 and 031 did,
because without them the battery had no UI. The owner's sign-off is still to be recorded here; a change
it asks for is a follow-up. Where the build differs from a wireframe:

- Safety → Battery group: shown on a board with a gauge, or while the rule is on (it then carries the
  D-47 "Reports unsafe" note); hidden otherwise, so boards without a battery gain no rows.
- Alerts → Battery group: shown only on a board with a gauge (six muted D-48 rows on every other board
  would be noise); D-48 still applies to the rows when alerts are off.
- Settings → Battery card: the D-46 note is said once, under **Monitor battery**, rather than on every
  muted row; the card is left out on firmware without a `battery` section.
- Glance tile: no "Open settings" link (a low pack isn't fixed in Settings).
- Times use the UI's existing duration format (`13h 17m`), not `13 h 17 min`.
