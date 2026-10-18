# Mockups: battery

Every state in [contracts/ui.md](../contracts/ui.md). These are **captures of the built UI**, not
drawings: the app's own components in the demo build (dark, the default theme), with the emulated
travel board (MAX17048 gauge, charger pins wired) put into each state through the demo's own
controls (charge level, USB power, charging fault, "Not responding", config). Each state has a
`-desktop.png` (1280 px viewport) and a `-phone.png` (390 px). Nothing here is shipped.

The demo board always has a charging pack, so a few states were captured with the device's
documents edited in the browser: no battery (b5), estimating (b6), the 99 h cap (b9), no gauge (y3,
s2, r3) and the Bluetooth build with a paired phone (r2). Everything else is the emulated device's
own answer. Nothing appears on a board without a fuel gauge (constitution V): no dashboard card,
glance tile, System rows or Alerts group, so those "no gauge" states have nothing to capture.

## Dashboard → Battery card (only with `battery.present`)

| File | State | DS rules |
|---|---|---|
| `b1-discharging` | Large `82`%, pill "Discharging"; tiles Voltage and Time left (`13h 13m`) | DS-01, DS-02, DS-03 |
| `b2-charging` | On USB: pill "Charging" (cyan), tile Time to full | DS-02, DS-03 |
| `b3-charged` | Pill "Charged" (green), `100`%, Voltage only | DS-02, DS-03 |
| `b4-fault` | Pill "Charging fault" (red) with "?" ("Too cold or too hot to charge"); percent and Voltage, no time tile | DS-02, DS-04 |
| `b5-no-battery` | Pill "No battery", no percent; Voltage while the gauge still answers | DS-02, DS-03 |
| `b6-estimating` | "Estimating" in place of the percent; time tile `--` | DS-03 |
| `b7-power-saving` | Below the critical level (8%): amber "Power saving" pill beside the state pill | DS-02 |
| `b8-unavailable` | Gauge stops answering: the existing fault card pattern titled "Battery", "Not responding", with the charger's pill | DS-02, DS-27 |
| `b9-over-99h` | Time left at the 5,940-minute cap: `> 99 h` | DS-03 |

## Dashboard → glance (Status card)

| File | State | DS rules |
|---|---|---|
| `g1-glance-low` | Below the low level (18%): note tile "Battery 18%" (amber) | DS-02 |
| `g2-glance-critical` | Below the critical level (8%): problem tile "Battery 8%" (red) | DS-02 |
| `g3-glance-fault` | Charging fault: problem tile "Charging fault" with "?" | DS-02, DS-04 |
| `g4-glance-going-to-sleep` | Below the shutdown level (4%), shutdown pending: problem tile "Going to sleep" | DS-02 |

Above the low level, or without a gauge: no tile.

## System → Sensors

| File | State | DS rules |
|---|---|---|
| `y1-system-battery` | Rows "Battery gauge MAX17048 v0x0012" (Detected), "Battery voltage 3.96 V", "Charger pins STAT1 H · STAT2 H · PG H" | DS-02, DS-03, DS-08 |
| `y2-system-gauge-error` | Gauge stops answering: badge "Not detected", no voltage row, charger pins still shown | DS-02, DS-08 |
| `y3-system-no-gauge` | No gauge: no battery rows at all | DS-08 |

## Settings → Device → Battery (`#battery`, after Display)

| File | State | DS rules |
|---|---|---|
| `s1-detected` | Badge "Detected"; Monitor battery; Low / Critical / Shutdown level (%) and Pack capacity (mAh) with "?"; Save power when low with Read sensors every / Warn before sleeping; Pins: Gauge alert, Charger STAT1, Charger STAT2, USB power (PG) | DS-01, DS-02, DS-04, DS-27 |
| `s2-not-detected` | Badge "Not detected"; D-46 "Inactive: No battery gauge detected" once under Monitor battery; every row but the pins muted | DS-02, DS-07 |
| `s3-validation` | Critical level 30 and Gauge alert on pin 21, then Save: the device's inline errors "2 to 25%, below the low level" and "Battery: pin is already in use" (D-51) | DS-06, DS-20 |

## Settings → Safety → Battery rule group

| File | State | DS rules |
|---|---|---|
| `r1-safety-battery-rule` | Unsafe below on (10%); Send an alert; Ring paired phones muted with D-50 "Needs the Bluetooth firmware build" (standard build) | DS-04, DS-06, DS-07 |
| `r2-safety-battery-rule-bluetooth` | Bluetooth build, passkey set, one paired phone: Ring paired phones active | DS-06 |
| `r3-safety-battery-rule-no-gauge` | Rule on without a gauge: D-47 fail-safe note "Reports unsafe: No battery gauge detected" | DS-07 |

With the rule off on a board without a gauge the group is hidden.

## Settings → Alerts → Battery events

| File | State | DS rules |
|---|---|---|
| `a1-alerts-battery-events` | "Battery" heading in the events card with six rows: Battery low, Battery critical, Going to sleep: battery empty, Power restored, Battery charged, Charging fault; each with level, Test and Text like every event | DS-01, DS-27 |

## OLED page

The display's Battery page (`Batt`, percent, `Chg`/`Full`/`Disch`/`Fault`/`No batt`) is rendered
with the other display pages in spec 031:
[oled-battery](../../031-oled-display/mockups/oled-battery.png),
[oled-battery-charging](../../031-oled-display/mockups/oled-battery-charging.png),
[oled-night-battery](../../031-oled-display/mockups/oled-night-battery.png).

## Review

Pending. The dashboard card, glance tile, System rows, Settings card, Safety row and Alerts group
(T028-T030, T040, T041, T054) were built from the earlier text wireframes in round 2, as specs 030
and 031 did, because without them the battery had no UI. The owner's sign-off is still to be
recorded here; a change it asks for is a follow-up. Where the build differs from a wireframe:

- Safety → Battery group: shown on a board with a gauge, or while the rule is on (it then carries the
  D-47 "Reports unsafe" note); hidden otherwise, so boards without a battery gain no rows.
- Alerts → Battery group: shown only on a board with a gauge (six muted D-48 rows on every other board
  would be noise); D-48 still applies to the rows when alerts are off.
- Settings → Battery card: the D-46 note is said once, under **Monitor battery**, rather than on every
  muted row; the card is left out on firmware without a `battery` section.
- Glance tile: no "Open settings" link (a low pack isn't fixed in Settings).
- Times use the UI's existing duration format (`13h 17m`), not `13 h 17 min`.
