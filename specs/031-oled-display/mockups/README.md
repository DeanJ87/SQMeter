# Mockups: OLED display

Every state in [contracts/ui.md](../contracts/ui.md). Wireframes for now (390 px phone width shown;
at 1280 px the card sits in the Device tab's two-column grid after Bluetooth, same rows). Rendered
`-phone.png` / `-desktop.png` captures drawn with the app's own markup and CSS (as spec 030 did) are
still to be added. Only existing parts are used: `SettingsCard`, `StatusBadge`, `Toggle`,
`DepToggle`, `NumberInput`, `SelectInput`, `ActionButton`, `ResultNote`, `Note`, "?" tips.

## Settings → Device → Display (`#display`)

### d1-detected (display on, found)

```
┌ Display ───────────────────────── [Detected] ┐
│ Show on display                        (on)  │
│ Pages                                        │
│  [x] Sky              [↑][↓]                 │
│  [x] Environment      [↑][↓]                 │
│  [ ] Cloud            [↑][↓]  ?              │
│  [x] Safety           [↑][↓]                 │
│  [ ] GPS              [↑][↓]  ?              │
│  [ ] Battery          [↑][↓]  ?              │
│  [ ] Network          [↑][↓]                 │
│ Change page every   [ 5 ] s                  │
│ Brightness          [ Medium ▾]  ?           │
│ Turn off after      [ 1 min ▾]               │
│ Light up at start-up                   (on)  │
│ Stay on at night                      (off)  │
│ Rotate 180°                           (off)  │
│ Wake button pin     [      ]  (empty = none) │
│ [ Wake ]                                     │
└──────────────────────────────────────────────┘
```

"?" beside a page whose source is absent: "No data source; skipped on the display". The Brightness
"?" says night always uses the lowest.

### d2-not-detected (on in settings, nothing at 0x3C)

Badge "Not detected" (dim). Every row but **Show on display** disabled with the D-43 note
"Inactive: No display detected." and its fix link; **Wake** disabled with the same reason.

### d3-off (display off in settings)

Badge "Off". Only **Show on display** shown enabled; the rest disabled (state Off, no warning).

### d4-stay-on (Stay on at night on, location set)

Under **Stay on at night**: one warn `Note` "Uses light at night" (US3 AC3). Without a location the
toggle is disabled with D-44 "Needs your location" and the Set location fix.

### d5-one-page (one page selected)

**Change page every** disabled (nothing to cycle).

### d6-validation (wake button pin clash)

Inline error under **Wake button pin**: "Display: wake button pin is already in use" (D-45; the
device's own message). An invalid GPIO: "Display: wake button pin is not a valid GPIO".

## System → Sensors

### y1-system-row

One row "Display" in the existing Sensors card: "Detected, on" / "Detected, off" / "Not detected" /
"Off". Nothing on the dashboard (the display is an output, not a sensor).

## Owner review

Pending. The Display card (T042), the System row (T043) and their strings wait for the owner's
sign-off here (constitution V: a reviewed mockup before UI is built). Everything behind them (config,
status, routes, settings dependencies, demo, docs) is built.
