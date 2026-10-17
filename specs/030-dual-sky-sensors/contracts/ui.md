# Contract: web UI

DS rules relied on: DS-01 (cards), DS-02 (state as one pill), DS-03 (MetricTile / ReadingRow),
DS-04 (reasons behind "?"), DS-06/07 (actions and notes in the card), DS-08 (one place per fact),
DS-09 (phone width), DS-20..25 (copy limits), DS-27 (one name: "SQM", "SQM-L"). Constitution V: no
card or tile for a disabled or undetected sensor. A mockup of every state below goes in
`specs/030-dual-sky-sensors/mockups/` for owner review before the components are built.

## Dashboard: Sky quality card (`SkyHero`, FR-012)

| State | Shows |
|---|---|
| One sensor enabled (every existing device) | exactly today's card |
| Both enabled, both ok, `showOther` on | name pill ("SQM-L") beside the Bortle pill; large value = authoritative; two extra tiles in the existing `metric-grid`: other reading (label "SQM" / "SQM-L") and "Difference" (signed, mag/arcsec²) |
| Both enabled, `showOther` off | name pill; no extra tiles |
| Both enabled, other not ok | name pill; no extra tiles (its fault shows on its glance tile) |
| Authoritative missing/faulted, other ok | the existing unavailable/fault card, title "Sky quality", pill with the missing sensor's state, a one-line note naming it ("SQM-L not responding") and an action "Use SQM" that saves `sky.authoritative` (FR-010) |
| Difference > 1.5 mag/arcsec² | both values and the difference still shown; the difference tile uses the amber tone (edge case) |

`LightCard` follows the authoritative sensor (unchanged). The MiniSpark history follows
the authoritative reading; it restarts when the source changes.

## At-a-glance (spec 025)

Expected sensors: `light` (the authoritative sensor, labelled with its name when both are
enabled) and `infrared`, plus a tile for the other sky sensor when it is enabled and has answered
since boot. A disabled sensor, or a non-authoritative one that has never answered (`missing` since
boot, "not detected", US1 AC3), has no tile; one that answered and then failed keeps a fault tile
(spec 025 D6). Fault text uses the sensor's name ("SQM-L not responding").

## Settings → Sensors

**Sky sensors card** (`#sky-sensors`):

- Rows "SQM" and "SQM-L" (model `TSL2591`), detected badge, enable `DepToggle` each.
  SQM-L's toggle is disabled with reason "Second I2C bus pins not set" and a link to the pin
  fields when I2C-2 is unset (D-39).
- Fields "I2C-2 SDA", "I2C-2 SCL" after the existing I2C fields (empty = not wired, saved as 255).
- "Authoritative reading" (`SelectInput`: SQM / SQM-L) and "Show other reading" (`DepToggle`),
  disabled with reason "SQM and SQM-L must both be on" when only one is enabled (D-40).
- When the saved authoritative sensor is off: one `Note` ("SQM-L is off; no sky reading") with an
  action "Use SQM" (FR-010).

**Sky quality card**: one per enabled sensor, titled "SQM" / "SQM-L" (one card titled "Sky
quality" as today when only the SQM is enabled). Same fields, ranges and dark-calibration flow;
each card binds its own config section and calls the route with its `sensor`.

## Alerts settings

The variable picker lists `{sqm_wide}`, `{sqm_l}`, `{sqm_delta}`, `{sqm_source}` with short
descriptions.

## Strings

Every new string in `web/src/i18n/en.json` with a context note in `en.context.json`, the names in
`tools/i18n/glossary/en.json`, all 13 locales translated, `tools/i18n/check.mjs` and
`gen_device_catalog.py --check` pass (FR-017).
