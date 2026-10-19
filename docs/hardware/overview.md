---
description: "SQMeter hardware: an ESP32 dev board and I²C sensors (TSL2591, MLX90614, BME280), on a breadboard or perfboard today."
---

# Hardware Overview

SQMeter is built around an ESP32 development board and a small set of I2C sensors. The electronics are straightforward and can be assembled on a breadboard or perfboard today, with a custom PCB coming in a future hardware release.

---

## Components

| Component | Role | Interface | Notes |
|-----------|------|-----------|-------|
| ESP32 dev board | Main MCU, WiFi, web server | — | — |
| TSL2591 | Sky brightness (lux / SQM) | I2C `0x29` | — |
| Second TSL2591, the SQM-L (optional) | Narrow-lens sky brightness | Second I2C bus, `0x29` | Optional — its own bus, as the address is fixed; see [Two sky sensors](../reference/sky-quality.md#two-sky-sensors-sqm-and-sqm-l) |
| BME280 | Temperature, humidity, pressure | I2C `0x76` | — |
| MLX90614 | IR cloud temperature | I2C `0x5A` | — |
| SSD1306 128 × 64 OLED (optional) | Readings at the pier | I2C `0x3C` | Optional — the OLED port (J6) on Rev A, or the main I2C pins; see [OLED display](../user-guide/display.md) |
| GPS module (optional) | Location and precise time | UART | Optional |
| RG-15 (optional) | Rain detection | UART | Optional — active development |
| Anemometer and wind vane (optional) | Wind speed, gust, direction | Pulse input + ADC1 | Optional — see [Wind](wind.md) |
| Li-ion pack, charger and MAX17048 fuel gauge (travel board) | Battery power in the field | I2C `0x36` + charger STAT1/STAT2/PG | Optional — see [Travel board](#travel-board-battery) and [Battery](../user-guide/battery.md) |

The ESP32 is the only component with WiFi. The core sensors talk to it over I²C; the optional GPS and RG-15 use serial (UART) and the wind sensors a pulse input and an analogue pin. The GPS module is optional — NTP can be used for time sync without it. See the [wiring overview](../getting-started/hardware.md) for the default pins.

---

## Travel board (battery)

The travel board (hardware Rev A, SQMeter-Hardware) runs SQMeter from a single-cell Li-ion pack, for
example two matched 18650s in parallel (about 7 Ah), so it can go to a dark site without mains:

| Part | What it does |
|---|---|
| MCP73871 power-path charger | Charges from USB-C at 833 mA and runs the device from USB while it charges; a thermistor stops charging below about 3 °C or above about 44 °C. Its STAT1, STAT2 and PG outputs (open drain, active low) say charging, charged, fault and USB power |
| MAX17048 fuel gauge | Charge (%), cell voltage and charge rate, on the main I2C bus at `0x36`; an ALRT output |
| DW01A + AO3400A protection | Cuts the cell off when it is over-discharged (about 2.4 V), over-charged or shorted |
| Power switch | Turns the device off; charging carries on |

On Rev A the gauge sits on I2C GPIO 8/9, ALRT on GPIO 1 and STAT1/STAT2/PG on GPIO 2/4/5. The firmware
doesn't assume these pins: the gauge is found on the main I2C bus by itself, and the ALRT and charger
pins are set in **Settings → Device → Battery** (all unset by default). Without the charger pins the
charge state comes from the gauge's rate, and *Charged*, *Charging fault* and *No battery* can't be
told. See [Battery](../user-guide/battery.md).

!!! warning "Cells and polarity"
    Use matched, protected cells of the same make, age and charge when putting two in parallel, and
    check the polarity before connecting the pack: a cell connected the wrong way round can damage the
    board.

---

## Hardware repository

A dedicated hardware repository is planned for all PCB design files, gerbers, schematics, and manufacturing assets.

!!! note "Coming soon — SQMeter-Hardware"
    Raw design files (KiCad project, gerbers, drill files, pick-and-place, BOM) will live in a separate **SQMeter-Hardware** repository rather than in this software repo. This keeps firmware and web UI releases clean and avoids bloating the main repo with large binary assets.

    The main docs will link to generated preview assets (schematic SVG, PCB renders, BOM) published by the hardware repo's CI pipeline.

---

## What you can build today

The current software is fully functional with off-the-shelf breakout boards wired over I2C. See [Hardware Setup](../getting-started/hardware.md) for wiring details.

No custom PCB is required to get started.
