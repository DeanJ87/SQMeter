# Hardware Setup

## Supported Sensors

| Sensor | Interface | Default Address | Purpose |
|--------|-----------|-----------------|---------|
| TSL2591 | I2C | 0x29 | High-sensitivity light / lux measurement |
| BME280 | I2C | 0x76 | Temperature, humidity, pressure |
| MLX90614 | I2C | 0x5A | IR temperature for cloud detection |
| GPS module | UART | n/a | Optional time sync and location |

All three I2C sensors share the same two-wire bus.

## I2C Wiring

| ESP32 Pin | Signal | Notes |
|-----------|--------|-------|
| GPIO 21 | SDA | Configurable via `sensor.sdaPin` |
| GPIO 22 | SCL | Configurable via `sensor.sclPin` |
| 3.3V | VCC | All three sensors run at 3.3V |
| GND | GND | Common ground |

!!! warning
    The ESP32 operates at 3.3V logic. Do not connect sensors or modules that require 5V I2C logic directly without a level shifter.

Pull-up resistors (4.7kΩ typical) are required on SDA and SCL. Most breakout boards include them.

## Sensor Addresses

### TSL2591 (Light Sensor)

- Fixed I2C address: **0x29**
- No address selection pins
- Provides: lux (calibrated), raw visible, raw IR, raw full-spectrum counts

### BME280 (Environmental Sensor)

- Default I2C address: **0x76** (SDO pin to GND)
- Alternative address: **0x77** (SDO pin to VCC)
- Provides: temperature (°C), relative humidity (%), pressure (hPa)

### MLX90614 (IR Cloud Detection)

- Default I2C address: **0x5A**
- Provides: ambient temperature and object (sky-pointing) temperature
- Mount facing skyward with an unobstructed field of view (~90° FOV)

## GPS Wiring

| GPS Module Pin | ESP32 Pin | Notes |
|----------------|-----------|-------|
| TX | GPIO 16 (RX) | Configurable via `gps.rxPin` |
| RX | GPIO 17 (TX) | Configurable via `gps.txPin` |
| VCC | 3.3V or 5V | Check module spec |
| GND | GND | Common ground |

Default baud rate: **9600** (configurable via `gps.baudRate`).

Common GPS modules confirmed working: u-blox NEO-6M, NEO-7M, NEO-8M; Quectel L80/L86.

## Power Requirements

| Component | Typical Current |
|-----------|----------------|
| ESP32 (WiFi active) | ~80 mA |
| TSL2591 | ~0.6 mA |
| BME280 | ~0.7 mA |
| MLX90614 | ~1.5 mA |
| GPS module | ~25–45 mA (acquisition), ~15 mA (tracking) |
| **Total (no GPS)** | **~85 mA @ 3.3V** |
| **Total (with GPS)** | **~120–130 mA @ 3.3V** |

Supply voltage: **5V** via USB or regulated supply. The ESP32 dev board regulates down to 3.3V for the MCU and sensors.

## Enclosure Recommendations

- Mount MLX90614 facing skyward through a weatherproof window or aperture
- Keep TSL2591 shielded from direct artificial light sources
- Ensure ventilation for BME280 humidity accuracy
- Use conformal coating on the PCB if deployed outdoors
