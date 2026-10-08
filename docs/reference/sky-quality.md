# Sky Quality Calculations

SQMeter converts TSL2591 light readings into three astronomical metrics. The conversion is only as good as the optical build and calibration: a bare TSL2591 is not a calibrated SQM instrument because it has a very wide angular response and can collect stray light from the horizon, ground, buildings, vehicles, and the enclosure.

---

## SQM — Sky Quality Meter

Measures sky brightness in magnitudes per square arcsecond (mag/arcsec²). Higher is darker.

```
SQM = 12.6 − 2.5 × log₁₀(lux)
```

Light below 0.0001 lux is treated as 0.0001 lux (SQM 22.6), the darkest the formula reports.

Firmware averages raw TSL2591 counts before this conversion. The night SQM path uses MAX gain and 600 ms integration, applies a rolling average, subtracts the saved dark visible offset, then applies the optional SQM calibration offset.

Dark calibration should be repeated whenever the lens, baffle, aperture, lens-to-sensor distance, or internal finish changes.

Typical values:

| Location | SQM |
|----------|-----|
| Excellent dark site | > 22 |
| Rural sky | ~21.5 |
| Suburban sky | ~19–20 |
| City centre | < 17 |

---

## NELM — Naked Eye Limiting Magnitude

The faintest star visible to the naked eye. SQMeter estimates it from SQM with Unihedron's formula:

```
NELM = 7.93 − 5 × log₁₀(10^(4.316 − SQM/5) + 1)
```

Below SQM 15 (twilight, indoors, city lights) NELM is reported as 0 - no stars visible. Higher = more stars visible. A dark rural sky gives NELM ≈ 6.5. Suburban skies typically give 4–5.

---

## Bortle Dark Sky Scale

Each class starts at its lower bound (inclusive): SQM 21.99 is class 1, 21.98 class 2.

| Class | SQM | Description |
|-------|-----------|-------------|
| 1 | ≥ 21.99 | Excellent dark-sky site |
| 2 | 21.89 – < 21.99 | Typical truly dark site |
| 3 | 21.69 – < 21.89 | Rural sky |
| 4 | 20.49 – < 21.69 | Rural/suburban transition |
| 5 | 19.50 – < 20.49 | Suburban sky |
| 6 | 18.94 – < 19.50 | Bright suburban sky |
| 7 | 18.38 – < 18.94 | Suburban/urban transition |
| 8 | 17.00 – < 18.38 | City sky |
| 9 | < 17.00 | Inner-city sky |

---

## Cloud Detection (MLX90614)

When an MLX90614 IR thermometer is present, SQMeter estimates cloud cover by comparing sky IR temperature to ambient temperature. A large negative delta (sky much colder than ambient) indicates clear sky. A small delta indicates cloud cover blocking the sky's thermal emission.

Humid air radiates more, making a clear sky look warmer, so the delta is corrected for humidity before it's classified:

```
delta     = sky temperature − ambient temperature
corrected = delta − (k / 100) × humidity          (humidity clamped to 0–100 %)
```

| Corrected delta | Condition | Cloud cover |
|---|---|---|
| below **Clear below** (default −13 °C) | Clear | 0 % |
| from Clear below up to **Overcast above** (default −3 °C) | Cloudy | linear 0 → 100 % |
| at or above Overcast above | Overcast | 100 % |

`k` is **Humidity correction** (default 0.75). All three are in **Settings → Sensors → Cloud detection**.

Humidity comes from the BME280. Without a working BME280 the model assumes **53 %** and says so: `clouds.humiditySource` is `assumed` in the readings, and the safety verdict counts the environment sensor as faulted. Alpaca's Humidity property never reports the assumed value.

This is a heuristic and works best in dry climates; fog and haze affect it. The model is computed once per reading, so the dashboard, MQTT, Alpaca and the safety verdict always agree.

The dew point is the Magnus formula (a = 17.27, b = 237.7 °C).

---

## Implementation

`lib/SkyLogic` - `SkyQuality.cpp`, `CloudDetection.cpp` and `Dewpoint.cpp` - with native tests in `test/test_sky_logic`.
