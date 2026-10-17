# Contract: configuration (`GET/POST /api/config`, saved config)

Additive. Every existing key keeps its name, type, range and meaning; `skyAveraging` and
`skyCalibration` are the SQM's (FR-003). A config without the new keys loads with the defaults
below (FR-014).

```json
{
  "sensor": {
    "readIntervalMs": 5000, "i2cSDA": 21, "i2cSCL": 22, "i2cFrequency": 100000,
    "i2c2SDA": 255,
    "i2c2SCL": 255
  },
  "sky": {
    "sqmEnabled": true,
    "sqmlEnabled": false,
    "authoritative": "sqm",
    "showOther": true
  },
  "skyAveraging":   { "windowSeconds": 90 },
  "skyCalibration": { "enabled": false, "sqmOffset": 0, "darkVisibleOffset": 0, "darkFullOffset": 0,
                      "darkIrOffset": 0, "darkSampleCount": 0, "darkCalibratedAt": 0 },
  "skyAveragingL":   { "windowSeconds": 90 },
  "skyCalibrationL": { "enabled": false, "sqmOffset": 0, "darkVisibleOffset": 0, "darkFullOffset": 0,
                       "darkIrOffset": 0, "darkSampleCount": 0, "darkCalibratedAt": 0 }
}
```

## Validation (messages are device text, catalogued by `tools/i18n/gen_device_catalog.py`)

| Rule | Message |
|---|---|
| `i2c2SDA`/`i2c2SCL` both 255, or both valid GPIOs and different | `Second I2C bus pins are invalid` |
| I2C-2 pins not used by I2C-1, GPS, rain sensor or anemometer (when on) | `Second I2C bus pins are already in use` |
| `sky.sqmlEnabled` needs I2C-2 pins | `SQM-L needs the second I2C bus pins` |
| `sky.authoritative` is `sqm` or `sqml` | `Authoritative sky reading must be sqm or sqml` |
| `skyAveragingL.windowSeconds` 10..300 | `SQM-L averaging window is invalid` |
| `skyCalibrationL.sqmOffset` -5..5 | `SQM-L calibration offset is invalid` |
| `skyCalibrationL` dark offsets >= 0, finite | `SQM-L dark calibration offsets are invalid` |

The wind pin conflict message ("Wind: pin is already used by I2C, GPS or the rain sensor") also
covers I2C-2 pins.

## Size

Worst-case main JSON (long strings in every text field, both sensors configured) stays under
`Config::MAX_PERSISTED_JSON_BYTES` (5100), asserted by a native test (FR-015).

## Effective settings / dependencies

`GET /api/settings/effective` reports the new entries D-39, D-40, D-41 (research D13) with the
existing shape.
