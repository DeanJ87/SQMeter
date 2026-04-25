# Schematic

!!! note "Coming soon"
    The schematic will be published here once the SQMeter-Hardware repository is live. This page will embed or link to a generated SVG/PDF exported by the hardware repo's CI pipeline — no raw KiCad files will be stored in this software repo.

---

## What the schematic covers

- ESP32 to TSL2591, BME280, and MLX90614 over shared I2C bus
- Optional GPS UART connection (RX/TX pins configurable in firmware)
- Power input and decoupling
- Status LED (optional)
- Reset and boot buttons

---

## What you can do now

For basic wiring, see [Hardware Setup](../getting-started/hardware.md). The I2C bus connections are described there with pin numbers for common ESP32 dev boards.

---

## Planned schematic publishing pipeline

The SQMeter-Hardware repo will use CI to generate and publish:

- **Schematic SVG** — embeddable in docs
- **Schematic PDF** — downloadable for printing
- **Interactive HTML BOM** — component placement reference

These will be published as GitHub Release assets from SQMeter-Hardware and linked here.
