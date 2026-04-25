# PCB

!!! note "Coming soon"
    A custom PCB design is in progress. Gerbers, drill files, pick-and-place files, and PCB renders will be published from the SQMeter-Hardware repository. This page will link to those assets once available.

---

## Design goals

- Single-board design integrating ESP32, TSL2591, BME280, and MLX90614
- Standard 2-layer PCB manufacturable at any JLC/PCBWay-class fab
- Minimal external components
- Footprint matched to the 3D-printed enclosure
- GPS header for optional module

---

## Manufacturing files

The hardware repo CI will generate and publish:

| File | Purpose |
|------|---------|
| Gerbers (zip) | Fabrication files for PCB manufacturer |
| Drill files | NC drill for vias and through-holes |
| Pick-and-place | SMT assembly reference |
| PCB top/bottom renders | Visual inspection reference |
| PCB 3D render | Enclosure fit verification |

---

## Building without a PCB

The firmware works with any ESP32 dev board and standard I2C breakout modules. See [Hardware Setup](../getting-started/hardware.md) for wiring on a breadboard or perfboard.
