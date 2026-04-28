# SQMeter

An ESP32-based dark sky quality monitoring system using TSL2591 (light), BME280 (environment), MLX90614 (IR cloud detection), and optional GPS.

## Quick Links

- [Build & Upload](getting-started/build.md) — How to compile and flash the firmware
- [Development Workflow](getting-started/dev-workflow.md) — Frontend and backend dev setup
- [API Reference](reference/api.md) — REST and WebSocket API
- [Configuration](reference/configuration.md) — All configurable parameters
- [Hardware Setup](reference/hardware.md) — Wiring and sensor addresses

## Key Features

- Real-time sky quality (SQM / NELM / Bortle scale)
- Cloud detection via IR temperature differential
- Web-based configuration and live dashboard
- MQTT integration
- OTA firmware updates
- GPS time sync (optional)
