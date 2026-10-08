# Wind (anemometer and vane)

SQMeter can read a standard reed-switch cup anemometer and an optional resistor-ladder wind vane - the type sold as the Misol WH-SP-WS01/WD, Argent 80422, SparkFun SEN-15901 weather meter, or the Davis 6410. Wind feeds ObservingConditions `windspeed`, `windgust` and `winddirection`, the Dashboard, and optional wind limits in the SafetyMonitor.

## Wiring

| Signal | ESP32 | Notes |
|---|---|---|
| Anemometer (2 wires) | GPIO 27 and GND (default pin) | The reed switch closes once per revolution (Misol: per ~2.4 km/h). Internal pull-up is enabled; a 100 nF capacitor from the pin to GND helps on long cables |
| Vane (2 wires) | GPIO 35 and GND (default pin), plus a **10 kΩ resistor from GPIO 35 to 3.3 V** | Must be an ADC1 pin (GPIO 32-39) - ADC2 can't be read while WiFi is on |

On the common RJ11 weather-meter cables, the anemometer uses the middle pair and the vane the outer pair - check your sensor's datasheet.

Keep these pins clear of I2C (21/22), GPS (16/17) and the RG-15 (18/19); **Settings → Sensors → Wind** rejects conflicts.

## Settings

**Settings → Sensors → Wind**:

- **Model** sets the speed per pulse frequency: 2.4 km/h per Hz (Misol/Argent/SparkFun) or 3.621 km/h per Hz (Davis 6410, 2.25 mph per Hz), or a custom value
- **Wind vane**: pin, pull-up value, and a **north offset** if the vane isn't mounted with its north mark pointing north

Changes apply immediately, without a restart.

## What's reported

| Value | Definition |
|---|---|
| Speed | Mean of the last 2 minutes (as METAR/AWOS report it) |
| Gust | Highest 3-second mean in the last 10 minutes (WMO definition) |
| Direction | Speed-weighted circular mean over the last 2 minutes; `0` when calm, per the Alpaca spec |

Pulses are debounced in software (2 ms), and sampled once a second. A vane reading that matches none of its 16 positions for 10 seconds (open or shorted wiring) is reported as a vane fault.

## Safety limits

**Settings → Safety → Safety rules → Wind** has optional maximum wind speed and maximum gust limits (m/s). Like rain, they're checked even when the other sensors' data is stale. If a wind limit is enabled but the anemometer is disabled or not reporting, the SafetyMonitor reports unsafe - a limit you can't measure isn't a limit you can trust.
