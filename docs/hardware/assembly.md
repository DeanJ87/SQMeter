# Assembly

!!! note "Coming soon"
    Full assembly instructions with photos will be published here once the PCB and enclosure designs are finalised. This page will cover PCB assembly, sensor mounting, enclosure fit, and first power-on.

---

## What to expect

The assembly guide will cover:

- **PCB assembly** — SMT component placement and soldering notes (or ordering pre-assembled from a fab)
- **Through-hole and headers** — GPS header, programming header, sensor connectors
- **Enclosure assembly** — heat-set insert installation, PCB mounting, lid closure
- **Cable routing** — power input, GPS antenna (if used)
- **First power-on** — what to expect before firmware is flashed

---

## Building today (breadboard)

You can use SQMeter right now without the custom PCB. Wire the sensors to your ESP32 dev board over I2C:

| Sensor | SDA | SCL | VCC | GND |
|--------|-----|-----|-----|-----|
| TSL2591 | GPIO 21 | GPIO 22 | 3.3 V | GND |
| BME280 | GPIO 21 | GPIO 22 | 3.3 V | GND |
| MLX90614 | GPIO 21 | GPIO 22 | 3.3 V | GND |

All three sensors share the same I2C bus. Default pins are GPIO 21 (SDA) and GPIO 22 (SCL) — both are configurable in the web UI under Settings → Sensors.

See [Hardware Setup](../getting-started/hardware.md) for the full wiring diagram.
