---
description: "Connect a cup anemometer and wind vane to SQMeter for wind speed, gusts and direction in the safety rules and ASCOM Alpaca."
---

# Wind (anemometer and vane)

SQMeter can read a standard reed-switch cup anemometer (the type sold as the Misol WH-SP-WS01, Argent 80422, SparkFun SEN-15901 weather meter, or the Davis 6410) and an optional wind vane with an AS5600 magnetic angle sensor. Wind feeds ObservingConditions `windspeed`, `windgust` and `winddirection`, the Dashboard, and optional wind limits in the SafetyMonitor.

## Wiring

| Signal | ESP32 | Notes |
|---|---|---|
| Anemometer (2 wires) | GPIO 27 and GND (default pin) | The reed switch closes once per revolution (Misol: per ~2.4 km/h). Internal pull-up is enabled; a 100 nF capacitor from the pin to GND helps on long cables |
| Vane: AS5600 (4 wires) | 3.3 V, GND, SDA and SCL of the I²C bus (GPIO 21/22) | Address `0x36`, alongside the sky sensors. Tie the AS5600's **DIR pin to GND** so the angle rises clockwise, and power it from 3.3 V: at 5 V its own pull-ups (if the board has them) would put 5 V on the ESP32's pins |

Keep the anemometer pin clear of I2C (21/22), GPS (16/17) and the RG-15 (18/19); **Settings → Sensors → Wind** rejects conflicts.

The vane shares the I²C bus with the sky sensors, so keep its cable short (a metre or two), run SDA and SCL on separate pairs, each twisted with GND or 3.3 V, and keep the bus at 100 kHz. A damaged vane cable can stop the whole bus.

Mount the magnet (diametrically magnetised, such as the 4 × 2 mm disc sold with AS5600 boards) on the vane's axis, 0.5 to 3 mm above the chip.

## Settings

**Settings → Sensors → Wind**:

- **Model** sets the speed per pulse frequency: 2.4 km/h per Hz (Misol/Argent/SparkFun) or 3.621 km/h per Hz (Davis 6410, 2.25 mph per Hz), or a custom value
- **Wind vane**: on or off, and a **north offset**, added to the reading: if the vane reads 30° while it points north, set -30

Changes apply immediately, without a restart.

## What's reported

| Value | Definition |
|---|---|
| Speed | Mean of the last 2 minutes (as METAR/AWOS report it) |
| Gust | Highest 3-second mean in the last 10 minutes (WMO definition) |
| Direction | Speed-weighted circular mean over the last 2 minutes; `0` when calm, per the Alpaca spec |

Pulses are debounced in software (2 ms), and sampled once a second. The AS5600 is read once a second too. If it doesn't answer, or sees no magnet, for 10 seconds, it's reported as a vane fault and the direction is left out. A magnet that's too weak (too far from the chip) or too strong (too close) still gives a direction, but Settings shows a warning so you can adjust the gap. `/api/status` reports the magnet status (`sensors.wind.vaneMagnet`) and the chip's gain (`vaneAgc`), which rises as the magnet moves away.

## Safety limits

**Settings → Safety → Safety rules → Wind** has optional maximum wind speed and maximum gust limits (m/s). Like rain, they're checked even when the other sensors' data is stale. If a wind limit is enabled but the anemometer is disabled or not reporting, the SafetyMonitor reports unsafe - a limit you can't measure isn't a limit you can trust.
