# Hardware Setup

SQMv2 runs on any standard ESP32 dev board. All sensors connect via I2C.

---

## Wiring

Default I2C pins (configurable in settings):

| ESP32 Pin | Function | Sensor Pins |
|-----------|----------|-------------|
| GPIO 21 | SDA | SDA on all sensors |
| GPIO 22 | SCL | SCL on all sensors |
| 3.3V | Power | VIN / VCC |
| GND | Ground | GND |

All sensors share the same two-wire I2C bus. Wire them in parallel.

```
ESP32          TSL2591     BME280      MLX90614
 3V3  ─────────  VIN ──────  VCC ──────  VCC
 GND  ─────────  GND ──────  GND ──────  GND
 G21  ─────────  SDA ──────  SDA ──────  SDA
 G22  ─────────  SCL ──────  SCL ──────  SCL
```

---

## I2C Addresses

| Sensor | Default Address | Notes |
|--------|----------------|-------|
| TSL2591 | `0x29` | Fixed |
| BME280 | `0x76` | `0x77` if SDO pulled high |
| MLX90614 | `0x5A` | Fixed |

!!! warning "Address conflict"
    If you have multiple BME280s on the same bus, one must be wired with SDO to 3.3V to use address `0x77`.

---

## Sensor Placement

For accurate sky readings:

- **TSL2591** should have a clear, unobstructed view of the sky — point it straight up
- **MLX90614** measures cloud temperature and should also face the sky
- **BME280** should be shaded from direct sunlight and have airflow — don't enclose it tightly
- Keep all sensors away from artificial light sources

---

## Verified Hardware

| Component | Notes |
|-----------|-------|
| ESP32 DevKit v1 | 30-pin or 38-pin, both work |
| Adafruit TSL2591 | Breakout board recommended |
| Adafruit BME280 | Breakout board recommended |
| Adafruit MLX90614 | Optional — for cloud detection |
| u-blox NEO-6M GPS | Optional — for location/time sync |

!!! tip "Power"
    Power the ESP32 via USB or a 5V supply. The 3.3V regulator on most dev boards can comfortably drive all sensors simultaneously.
