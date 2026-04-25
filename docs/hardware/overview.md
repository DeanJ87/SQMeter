# Hardware Overview

SQMeter is built around an ESP32 development board and a small set of I2C sensors. The electronics are straightforward and can be assembled on a breadboard or perfboard today, with a custom PCB coming in a future hardware release.

---

## Components

| Component | Role | Interface |
|-----------|------|-----------|
| ESP32 dev board | Main MCU, WiFi, web server | — |
| TSL2591 | Sky brightness (lux / SQM) | I2C `0x29` |
| BME280 | Temperature, humidity, pressure | I2C `0x76` |
| MLX90614 | IR cloud temperature | I2C `0x5A` |
| GPS module (optional) | Location and precise time | UART |

The ESP32 is the only component with WiFi. Everything else talks to it over I2C. The GPS module is optional — NTP can be used for time sync without it.

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
