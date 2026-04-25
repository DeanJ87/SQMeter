# Bill of Materials

!!! note "Coming soon"
    A machine-generated BOM will be published here from the SQMeter-Hardware repository. The BOM will be exported from KiCad and published as a CSV (and optionally as a human-readable HTML page) via the hardware repo's CI pipeline. It will not be manually maintained here.

---

## Key components

This is a rough component list for planning purposes — not a complete BOM. Quantities and exact part numbers will be in the generated BOM.

| Component | Description | Notes |
|-----------|-------------|-------|
| ESP32 | 38-pin dev board (e.g. DOIT ESP32 DEVKIT V1) | Or equivalent 30-pin |
| TSL2591 | High-dynamic-range light sensor breakout | Adafruit or equivalent |
| BME280 | Temperature / humidity / pressure breakout | 3.3 V version |
| MLX90614 | IR thermometer breakout | GY-906 or equivalent |
| GPS module | u-blox NEO-6M or compatible | Optional |
| Enclosure | 3D-printed — see [Enclosure](enclosure.md) | PETG or ASA |
| M3 heat-set inserts | Lid fasteners | 4× |
| M3×8 screws | Lid screws | 4× |

---

## Where to buy

Common suppliers for the sensor breakouts: Adafruit, SparkFun, AliExpress, LCSC.

The full BOM with supplier links and part numbers will be in the generated artifact from SQMeter-Hardware.

---

## BOM generation strategy

The hardware repo CI will:

1. Export `bom.csv` from KiCad during the build
2. Optionally generate `bom.html` (human-readable interactive BOM via InteractiveHtmlBom)
3. Publish both as GitHub Release assets on SQMeter-Hardware
4. This docs page will link to those assets once the hardware repo is live
