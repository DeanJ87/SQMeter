# Mockups: OLED display

Every state in [contracts/ui.md](../contracts/ui.md) and every page in
[contracts/screen.md](../contracts/screen.md). These are **captures of the built UI**, not drawings:
the web states are the app's own components in the demo build (dark, the default theme), with the
emulated device put into each state; each has a `-desktop.png` (1280 px viewport) and a `-phone.png`
(390 px). The OLED pages are the firmware's own pixels (see below). Nothing here is shipped.

The demo board always has a display, so "Not detected" (d2, y3) and "Detected, off" (y2) were
captured with the device's status and settings-facts documents edited in the browser; everything
else is the emulated device's own answer.

## Settings → Device → Display (`#display`)

| File | State | DS rules |
|---|---|---|
| `d1-detected` | Display on and found: badge "Detected"; chosen pages first with up/down order buttons, then the others; "?" beside GPS (no data source on the demo board, still selectable); Change page every / Brightness / Turn off after; Light up at start-up, Stay on at night, Rotate 180°; Wake button pin; Wake | DS-01, DS-02, DS-04, DS-27 |
| `d2-not-detected` | On in settings, nothing at 0x3C: badge "Not detected"; D-43 "Inactive: No display detected" with its fix link under **Show on display**; every other control and **Wake** muted | DS-02, DS-06, DS-07 |
| `d3-off` | Display off in settings: badge "Off"; only **Show on display** usable, the rest muted without a warning colour (the demo reports no display while it's off, so Stay on at night names that) | DS-02, DS-07 |
| `d4-stay-on` | Stay on at night on, location set: warn note "Uses light at night" (US3 AC3) | DS-07, DS-20 |
| `d4b-stay-on-no-location` | No location: Stay on at night blocked with D-44 "Needs your location." and Set location | DS-04, DS-06 |
| `d5-one-page` | One page chosen: **Change page every** muted (nothing to cycle) | DS-07 |
| `d6-validation` | Wake button pin on a pin in use (21, I2C SDA) and Save: the device's inline error "Display: wake button pin is already in use" (D-45) | DS-06, DS-20 |

At 1280 px the card spans the Device tab's single column; at 390 px the fields stack.

## System → Sensors

| File | State | DS rules |
|---|---|---|
| `y1-system-display-on` | Row "Display": "Detected, on" | DS-02, DS-08 |
| `y2-system-display-off` | "Detected, off" (timed out or blanked) | DS-02, DS-08 |
| `y3-system-display-not-detected` | "Not detected" (red) | DS-02, DS-08 |
| `y4-system-display-disabled` | "Off" (switched off in Settings) | DS-02, DS-08 |

Nothing on the dashboard: the display is an output, not a sensor (constitution V).

## OLED pages

128 × 64 monochrome, scaled 4× with nearest-neighbour (512 × 256), lit pixels white on black (the
SSD1306 panel the firmware drives; a yellow/blue panel shows the same pixels in its colours). Each
image is the frame drawn by the firmware's own `lib/DisplayLogic` (`render()` in `DisplayPages.cpp`
with `DisplayFrame.cpp` and `DisplayFonts.cpp`), compiled natively and dumped pixel by pixel, so
it matches the device exactly. Burn-in shift step 0 unless noted. Device name `SQM-ESP32` (the
default), setup access point `SQM-Setup`.

| File | Page / state |
|---|---|
| `oled-sky` | Sky, one sky sensor: `SQM mag`, `21.34`, `Bortle 3  NELM 6.4` |
| `oled-sky-two-sensors` | Sky with the other sky sensor reporting: extra line `SQM-L 21.10` |
| `oled-environment` | `Temp`, `12.4°C`, `Hum 78%  Dew 8.6°C` |
| `oled-cloud` | `Sky`, `-18.2°C`, `Cloud clear` |
| `oled-safety-safe` | `Safe` |
| `oled-safety-unsafe` | `UNSAFE` as an inverse block, first reason `SQM 18.21 < 19.50` |
| `oled-gps` | `GPS`, `Fix`, `Sats 9`, `51.5074 -0.1278` |
| `oled-gps-no-fix` | `No fix`, `Sats 3`, `-- --` |
| `oled-battery` | `Batt`, `82%`, `Disch` (spec 032) |
| `oled-battery-charging` | `64%`, `Chg` |
| `oled-network` | Device name, `WiFi`, `IP 192.168.1.128` |
| `oled-network-setup-ap` | Setup access point: `AP SQM-Setup`, `IP 192.168.4.1` |
| `oled-network-no-wifi` | No connection: `WiFi --`, no address |
| `oled-no-data-sky` | No-data marker: missing/faulted/stale values drawn as `--` (FR-005) |
| `oled-no-data-environment` | `--` for temperature, humidity and dew point |
| `oled-no-data-safety` | Safety unknown: `--` |
| `oled-night-sky` | Night: one large value, centred (`21.34`) |
| `oled-night-environment` | Night: `12.4°C` |
| `oled-night-cloud` | Night: `-18.2°C` |
| `oled-night-safety-unsafe` | Night: `UNSAFE` as plain text, no lit block |
| `oled-night-gps` | Night: satellites (`9`) |
| `oled-night-battery` | Night: `82%` |
| `oled-night-network` | Night: last octets `.1.128` |
| `oled-night-network-setup-ap` | Night, setup access point: `AP` |
| `oled-sky-shift` | Sky at burn-in step 2: everything 1 px right and 1 px down |

Night's "minimum contrast" is the panel's contrast setting, not pixels, so it doesn't show here.
The contract's "outline text" for night `UNSAFE` is drawn as plain text by the build (no lit block
either way). The other sky reading uses the same small font as the lines above it.

## Owner review

Pending. The Display card (T042) and the System row (T043) were built from the earlier text
wireframes in round 2, as spec 030 did, because without them FR-015 had no Settings UI. The owner's
sign-off is still to be recorded here; a change it asks for is a follow-up. Where the build differs
from a wireframe: page order arrows show only on chosen pages (an unchosen page has no place in the
order), and with the display off the Wake button stays visible but disabled.
