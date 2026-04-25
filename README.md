# SQMeter

**ESP32 Dark Sky Quality Monitor**

[![Build](https://github.com/DeanJ87/SQMeter/actions/workflows/build.yml/badge.svg)](https://github.com/DeanJ87/SQMeter/actions/workflows/build.yml)
[![Docs](https://github.com/DeanJ87/SQMeter/actions/workflows/docs.yml/badge.svg)](https://github.com/DeanJ87/SQMeter/actions/workflows/docs.yml)
[![GitHub release](https://img.shields.io/github/v/release/DeanJ87/SQMeter?label=release)](https://github.com/DeanJ87/SQMeter/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/platform-ESP32-red.svg)](https://platformio.org/)

SQMeter measures light pollution in real time using an ESP32. It gives you SQM magnitude, Bortle class, NELM, cloud cover, temperature, humidity, and pressure — all accessible from any browser on your local network.

## Links

- **Docs:** https://deanj87.github.io/SQMeter/
- **Demo:** https://deanj87.github.io/SQMeter/demo/
- **Releases:** https://github.com/DeanJ87/SQMeter/releases

## Highlights

- TSL2591 light sensor — SQM, NELM, Bortle 1–9
- BME280 — temperature, humidity, pressure
- MLX90614 — IR cloud temperature and cloud cover estimate
- GPS support (optional) — location and precise time
- Real-time web dashboard over WebSocket
- REST API and MQTT publishing
- OTA firmware updates from the browser
- Captive portal Wi-Fi setup on first boot

## Quick Start

See [Flashing Your Device](https://deanj87.github.io/SQMeter/getting-started/flashing/) to get started with a new ESP32.
