# SQMeter

**ESP32 Dark Sky Quality Monitor**

[![Build](https://github.com/DeanJ87/SQMeter/actions/workflows/build.yml/badge.svg)](https://github.com/DeanJ87/SQMeter/actions/workflows/build.yml)
[![Docs](https://github.com/DeanJ87/SQMeter/actions/workflows/docs.yml/badge.svg)](https://github.com/DeanJ87/SQMeter/actions/workflows/docs.yml)
[![ASCOM ConformU](https://img.shields.io/github/actions/workflow/status/DeanJ87/SQMeter/alpaca-conformance.yml?branch=main&label=ASCOM%20ConformU&logo=githubactions&logoColor=white)](https://github.com/DeanJ87/SQMeter/actions/workflows/alpaca-conformance.yml)
[![ASCOM Alpaca](https://img.shields.io/badge/ASCOM%20Alpaca-SafetyMonitor%20v3%20%7C%20ObservingConditions%20v2-1f6feb)](docs/user-guide/alpaca.md)
[![GitHub release](https://img.shields.io/github/v/release/DeanJ87/SQMeter?label=release)](https://github.com/DeanJ87/SQMeter/releases/latest)
[![Beta](https://img.shields.io/github/v/release/DeanJ87/SQMeter?include_prereleases&label=beta&color=orange)](https://github.com/DeanJ87/SQMeter/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/platform-ESP32-red.svg)](https://platformio.org/)

SQMeter measures light pollution in real time using an ESP32. It gives you SQM magnitude, Bortle class, NELM, cloud cover, temperature, humidity, and pressure — all accessible from any browser on your local network.

## Links

- **Docs:** https://sqmeter.dev/
- **Demo:** https://demo.sqmeter.dev/
- **Releases:** https://github.com/DeanJ87/SQMeter/releases

## Highlights

- TSL2591 light sensor — SQM, NELM, Bortle 1–9
- BME280 — temperature, humidity, pressure
- MLX90614 — IR cloud temperature and cloud cover estimate
- GPS support (optional) — location and precise time
- RG-15 rain sensor and anemometer / wind vane support (optional)
- Native ASCOM Alpaca SafetyMonitor + ObservingConditions (N.I.N.A.-compatible, no bridge), tested with ConformU on every PR
- Safety rules for rain, wind, cloud, SQM, humidity and dew point, with the reasons shown and a safe delay
- Alerts via Pushover, ntfy, webhook, MQTT or a paired phone over Bluetooth — per-event levels and sounds, your own wording, only when it's dark, and an on/off switch for Home Assistant or N.I.N.A.
- Real-time dashboard with reorderable cards and a Sun & Moon night chart
- REST API and MQTT publishing (including a 1/0 safe flag for logging)
- OTA firmware updates from the browser, or self-updated directly from GitHub Releases
- Captive portal Wi-Fi setup on first boot

## Quick Start

See [Flashing Your Device](https://sqmeter.dev/getting-started/flashing/) to get started with a new ESP32.

## Current Security Model

SQMeter is intended for a trusted local network. The current firmware has no web/API authentication, unauthenticated OTA endpoints, and exposes saved configuration through the LAN API. Do not port-forward the device or place it on guest WiFi.
