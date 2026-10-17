# Contract: web UI

DS rules relied on: DS-01 (cards from the shared parts), DS-02 (state as one pill), DS-04 (reasons
behind "?"), DS-06/07 (actions and notes inside the card), DS-08 (one place per fact), DS-09 (phone
width), DS-20..25 (short English copy), DS-27 (one name: "Display"). Constitution V: no dashboard
card for the display (it is an output, not a sensor). A mockup of each state below (390 px and
1280 px) goes in `specs/031-oled-display/mockups/` and is reviewed by the owner before the card is
built.

## Settings → Device → Display card (`#display`, FR-015)

Order and controls (all existing components from `controls.tsx` / `ui.tsx`):

1. Title "Display", `StatusBadge`: "Detected" (ok) / "Not detected" (dim) / "Off" when disabled.
2. "Show on display" — `Toggle` for `display.enabled`.
3. "Pages" — one row per page (Sky, Environment, Cloud, Safety, GPS, Battery, Network): a checkbox
   and up/down order buttons (existing small icon `Button`s). A page whose source is absent shows a
   dim "No data source" note behind "?"; it stays selectable (it is skipped on the device).
4. "Change page every" — `NumberInput` seconds 2..60 (disabled when one page is selected).
5. "Brightness" — `SelectInput` Low / Medium / High; "?" says night always uses the lowest.
6. "Turn off after" — `SelectInput`: 10 s, 30 s, 1 min, 2 min, 5 min, 10 min, 30 min, Never.
7. "Light up at start-up" — `Toggle` (`wakeOnBoot`).
8. "Stay on at night" — `DepToggle` (D-44); when on, a `Note` tone warn: "Uses light at night"
   (US3 AC3).
9. "Rotate 180°" — `Toggle`.
10. "Wake button pin" — `NumberInput` (empty = none, saved as 255); validation error inline.
11. "Wake" — `ActionButton` calling `POST /api/display/wake`; result via `ResultNote`. Disabled with
    the D-43 reason when not detected.

When not detected, every control except the "Show on display" toggle is disabled with the D-43
reason ("No display detected") and its fix link; with the display enabled but undetected, the
badge reads "Not detected".

## Other pages

- Dashboard: nothing new. Inventory (`web/src/dashboard/inventory.json`): `status.display.*`
  recorded under "not shown" with reason "Output device; its state is on the Settings Display card".
- System page: one row "Display" in the existing Sensors card (`system.sensors`, `web/src/components/System.tsx`): "Detected, on" / "Detected,
  off" / "Not detected" / "Off" (DS-08: the Settings card is where it's changed).

## Strings

Every label, option, note and error above is added to `web/src/i18n/en.json` with a context note in
`en.context.json` and translated into the 13 other locales (FR-019). Device-built messages
(validation, route errors, D-43 reason) go through `tools/i18n/gen_device_catalog.py`.
