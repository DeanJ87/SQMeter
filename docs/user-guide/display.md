---
description: "An optional 128 x 64 OLED display on SQMeter: the sky reading and other pages at the pier, dim and short-lived at night, woken from the web UI, the API or a button."
---

# OLED display

SQMeter can drive a small **SSD1306 128 × 64 OLED** (the common 0.96" I2C module) so you can read
the sky at the pier without a phone. It is optional: without one, nothing changes.

## Fitting one

The display goes on the **main I2C bus** with the other sensors, at address **`0x3C`**
(see [Hardware Setup](../getting-started/hardware.md#oled-display-optional)): VCC to 3.3 V, GND,
SDA and SCL in parallel with the TSL2591, BME280 and MLX90614. On the Rev A boards it plugs into
the OLED port (J6).

!!! note "Address 0x3C"
    Another device answering at `0x3C` on the same bus would look like a display. SQMeter's own
    sensors use `0x29`, `0x76` and `0x5A`, so this only matters if you add something else there.

## Turning it on

Switch on **Show on display** on the **Display** card (Settings → Device; `display.enabled`), then save. No restart is needed. SQMeter looks for
the display when it is on, and again every 5 seconds until it answers; it is never probed while it is
off, so a device without one has no extra bus traffic. **System → Sensors** shows whether it was found.

| Setting | Default | What it does |
|---|---|---|
| Show on display | off | Uses the display at all |
| Pages | Sky | Which pages to show, in order (1 to 7) |
| Change page every | 5 s | 2 to 60 seconds; with one page it doesn't change |
| Brightness | Medium | Low, Medium or High by day; night always uses the lowest |
| Turn off after | 1 min | 10 s to 30 min, or Never (by day only, see below) |
| Light up at start-up | on | Lights the display after a restart, for the switch-off time |
| Stay on at night | off | Keeps it lit at night; needs your location |
| Rotate 180° | off | For a display mounted upside down |
| Wake button pin | none | A free GPIO for a push button to ground |

## Pages

A page whose data source is missing (no BME280, GPS off, no battery fitted) is skipped. If none of
the chosen pages can be shown, the Network page is, so you can always find the device. A value that
is missing, faulted or stale is shown as `--`, never as its last value.

| Page | By day | At night |
|---|---|---|
| Sky | `SQM mag` (or `SQM-L mag`, the sky sensor that counts), the reading, `Bortle` and `NELM`; the other sky sensor's reading below when it reports | The reading |
| Environment | `Temp`, the temperature, `Hum` and `Dew` | The temperature |
| Cloud | `Sky` (sky temperature), `Cloud clear` / `cloudy` / `overcast` | The sky temperature |
| Safety | `Safe` or `UNSAFE` (on a lit block when unsafe), and the first reason | `Safe` or `UNSAFE` |
| GPS | `GPS`, `Fix` or `No fix`, `Sats`, latitude and longitude | The number of satellites |
| Battery | `Batt`, the charge, `Chg` / `Full` / `Disch` / `Fault` / `No batt` | The charge |
| Network | The device name, `WiFi` and its `IP`, or `AP` and the setup hotspot's name and address | The end of the address (`.1.128`), or `AP` |

Text that doesn't fit its line ends in `…`. Temperatures are in °C and sky quality in mag/arcsec²,
as everywhere else on the device. The labels are always in English:
`SQM`, `SQM-L`, `mag`, `Bortle`, `NELM`, `Temp`, `Hum`, `Dew`, `Sky`, `Cloud`, `clear`, `cloudy`,
`overcast`, `Safe`, `UNSAFE`, `GPS`, `Fix`, `No fix`, `Sats`, `Batt`, `Chg`, `Full`, `Disch`, `Fault`,
`No batt`, `WiFi`, `AP`, `IP`, `--`, `…`.

Each page change moves the picture by one pixel (four positions in turn) so nothing burns in.

## At night

At night the display protects your dark adaptation:

- it uses the **night layout**: one large value per page, no lit blocks;
- it runs at the **lowest brightness** the panel has;
- it **always turns off** after the switch-off time: **Never** becomes 1 minute at night.

Night is the same as on the dashboard and for alerts: the sun below the **Darkness** limit
(Settings → Alerts, `alerts.nightSunAltitudeDeg`), worked out from a GPS fix or your location and the clock. **Until the device
knows both, it treats every hour as night**, so after a restart without a network the display is dim
and turns off. **Stay on at night** keeps it lit through a night the device can work out; without a
location it is kept as saved but has no effect (Settings says *Needs your location*).

## Waking it

- **Wake** on the **Display** card (Settings → Device) lights it for the switch-off time.
- `POST /api/display/wake` does the same, and `POST /api/display/blank` turns it off until the next
  wake ([REST API](../api/rest.md#post-apidisplaywake-post-apidisplayblank)).
- A **wake button**: a push button from a free GPIO to ground (internal pull-up), set as
  **Wake button pin**. A press while it is off lights it; a press while it is lit shows the next page
  and restarts the switch-off time. On the Rev A boards the AUX pins are free for this. The pin can't
  be one that I2C, the second I2C bus, GPS, the rain sensor, the anemometer and vane or the
  [battery](battery.md) pins (gauge alert, STAT1, STAT2, PG) use.

`/api/status` reports the display's state under `display` ([REST API](../api/rest.md#get-apistatus)).
