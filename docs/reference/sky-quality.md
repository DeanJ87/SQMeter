# Sky Quality Calculations

SQMeter converts raw lux from the TSL2591 into three astronomical metrics.

---

## SQM — Sky Quality Meter

Measures sky brightness in magnitudes per square arcsecond (mag/arcsec²). Higher is darker.

```
SQM = -2.5 × log₁₀(lux) + 12.59
```

Typical values:

| Location | SQM |
|----------|-----|
| Excellent dark site | > 22 |
| Rural sky | ~21.5 |
| Suburban sky | ~19–20 |
| City centre | < 17 |

---

## NELM — Naked Eye Limiting Magnitude

The faintest star visible to the naked eye under current conditions. Estimated from the SQM value using empirical formulae based on atmospheric conditions.

Higher = more stars visible. A dark rural sky gives NELM ≈ 6.5. Suburban skies typically give 4–5.

---

## Bortle Dark Sky Scale

| Class | SQM Range | Description |
|-------|-----------|-------------|
| 1 | > 21.99 | Excellent dark-sky site |
| 2 | 21.89 – 21.99 | Typical truly dark site |
| 3 | 21.69 – 21.89 | Rural sky |
| 4 | 20.49 – 21.69 | Rural/suburban transition |
| 5 | 19.50 – 20.49 | Suburban sky |
| 6 | 18.94 – 19.50 | Bright suburban sky |
| 7 | 18.38 – 18.94 | Suburban/urban transition |
| 8 | 17.00 – 18.38 | City sky |
| 9 | < 17.00 | Inner-city sky |

---

## Cloud Detection (MLX90614)

When an MLX90614 IR thermometer is present, SQMeter estimates cloud cover by comparing sky IR temperature to ambient temperature. A large negative delta (sky much colder than ambient) indicates clear sky. A small delta indicates cloud cover blocking the sky's thermal emission.

This is a heuristic and works best in dry climates. Humidity and fog affect the reading.

---

## Implementation

See `src/calculations/SkyQuality.cpp` and `src/calculations/CloudDetection.cpp`.
