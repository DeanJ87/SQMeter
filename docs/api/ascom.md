# ASCOM TCP Protocol

SQMeter includes a lightweight TCP server intended for ASCOM ObservingConditions-style integrations. It listens on port `2020` when enabled by firmware configuration and WiFi is connected.

!!! warning "Rain safety limitation"
    The current firmware's rain-rate command returns `0.0` even when an RG-15 sensor is enabled. Do not rely on the ASCOM/TCP rain value for roof or equipment safety until the firmware is updated and validated.

## Connection

Use a plain TCP socket:

```bash
nc sqm-esp32.local 2020
```

Commands are short colon-prefixed strings terminated with `#`. Responses use the firmware format `<value#`.

## Supported Commands

| Command | Meaning | Current response |
|---------|---------|------------------|
| `:003#` | Sky quality | SQM mag/arcsec2 from TSL2591-derived calculation |
| `:004#` | Sky brightness | Raw lux from TSL2591 |
| `:008#` | Firmware version | Firmware version string |
| `:009#` | Firmware name | `SQMeter` |
| `:028#` | Humidity | Percent from BME280 |
| `:029#` | Pressure | hPa from BME280 |
| `:030#` | Ambient temperature | Degrees C from BME280 |
| `:031#` | Dew point | Degrees C from BME280 |
| `:035#` | Sky temperature | Degrees C from MLX90614 object temperature |
| `:038#` | Cloud cover | Percentage from cloud detection |
| `:051#` | Rain rate | Currently always `0.0` |
| `:054#` | Wind speed | Not implemented; returns `0.0` |
| `:055#` | Wind direction | Not implemented; returns `0.0` |
| `:059#` | Wind gust | Not implemented; returns `0.0` |

The TCP protocol is intentionally minimal and does not implement the full Alpaca HTTP API. Validate command behavior against your running firmware before connecting observatory automation.

## Integration Guidance

- Poll slowly; avoid sub-second polling from multiple clients.
- Treat missing, fixed, or zero values as unavailable unless the corresponding sensor is known to be initialised.
- Keep TCP port `2020` on a trusted LAN. There is no TCP authentication.
