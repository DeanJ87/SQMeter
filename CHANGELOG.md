# Changelog

All notable changes to SQMeter are documented here.

## [Unreleased]

- Hardware PCB design (planned — SQMeter-Hardware repo)
- 3D-printed enclosure (planned — Printables)
- GPS time source implementation

## [0.0.1] — 2026-04-25

Initial alpha release.

### Firmware

- TSL2591 sky brightness sensor (SQM, NELM, Bortle 1–9)
- BME280 temperature, humidity, pressure
- MLX90614 IR cloud temperature and cloud cover estimate
- GPS module support (optional, u-blox NEO-6M compatible)
- NTP time sync with configurable servers and timezone
- MQTT publishing to any broker (Home Assistant, Grafana, etc.)
- OTA firmware and filesystem updates from the browser
- Captive portal Wi-Fi setup on first boot
- REST API and WebSocket live sensor feed
- Partition layout: dual OTA slots + LittleFS for web UI

### Web UI

- Real-time dashboard (SQM, Bortle, NELM, cloud cover, environment)
- System page (memory, flash, sensor status, NTP/GPS, MQTT status)
- Settings page (WiFi, NTP, GPS, MQTT, sensor intervals, I2C pins)
- OTA update page (firmware and filesystem upload with progress)
- Zod-validated settings form

### CI / CD

- GitHub Actions: firmware build on PR, release on tag
- GitHub Pages: docs + live demo deployed on merge to main
- Playwright screenshot pipeline for docs
- Complete flash image (bootloader + firmware + LittleFS) in releases
