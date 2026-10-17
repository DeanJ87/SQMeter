# Contract: what the screen shows

128 × 64 px, monochrome. Fonts: **S** = 5×7 (6 px advance, 8 px line, 21 chars/line), **L** = 10×16
digits (12 px advance) for the primary value. Labels are fixed English (FR-011); `--` is the
no-data marker; text wider than its line ends in `…` (1 glyph). Temperatures `°C`, sky quality
`mag`. Content shifts by 0/1 px in x and y in a 4-step pattern at each page change (burn-in).

Pixel-exact expectations live in the native tests (`test/test_display_logic`); the layouts below are
the contract they check.

## Day layout

| Page | Line 1 (S) | Primary (L) | Lines below (S) |
|---|---|---|---|
| Sky | `SQM` or `SQM-L` (authoritative name) + `mag` | `21.34` | `Bortle 3  NELM 6.4`; `SQM-L 21.10` when the other sky sensor reports (smaller type) |
| Environment | `Temp` | `12.4°C` | `Hum 78%  Dew 8.6°C` |
| Cloud | `Sky` | `-18.2°C` | `Cloud clear` / `cloudy` / `overcast` |
| Safety | — | `Safe` / `UNSAFE` (inverse block by day when unsafe) | first reason, cut with `…` (e.g. `SQM 18.21 < 19.50`); `--` when unknown |
| GPS | `GPS` | `Fix` / `No fix` | `Sats 9`; `51.5074 -0.1278` |
| Battery | `Batt` | `82%` | `Chg` / `Full` / `Disch` / `Fault` / `No batt` (spec 032) |
| Network | device name (cut with `…`) | — | `WiFi` + `IP 192.168.1.128` / `AP SQMeter-Setup` + `IP 192.168.4.1` / `WiFi --` (state) |

A value that is missing, faulted or stale is drawn as `--`, never as its last value (FR-005).

## Night layout (night or night unknown)

One primary value per page in the large font, at minimum contrast, no inverse blocks and no borders:
Sky → authoritative SQM; Environment → temperature; Cloud → sky temperature; Safety → `Safe` /
`UNSAFE` (outline text, not inverse); GPS → satellites; Battery → percent; Network → last octets of the IP
(`.1.128`) in the large font or `AP` (the full address is on the day layout and in the web UI).

## Label list (for the docs page, FR-011)

`SQM`, `SQM-L`, `mag`, `Bortle`, `NELM`, `Temp`, `Hum`, `Dew`, `Sky`, `Cloud`, `clear`, `cloudy`,
`overcast`, `Safe`, `UNSAFE`, `GPS`, `Fix`, `No fix`, `Sats`, `Batt`, `Chg`, `Full`, `Disch`,
`Fault`, `No batt`, `WiFi`, `AP`, `IP`, `--`, `…`.
