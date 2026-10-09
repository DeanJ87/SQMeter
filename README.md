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

## How it fits together

<!-- diagram: DIA-01
sources: src/main.cpp#setup include/sensors/ include/WebServer.h include/MQTTClient.h include/AlertDispatcher.h include/BleService.h include/OtaUpdater.h include/TimeManager.h
blocking: false
fingerprint: 85bdfbdf63a4a14b
-->
```mermaid
flowchart LR
    accTitle: What SQMeter connects to
    accDescr: Sensors feed the ESP32. It keeps its settings and web UI on the device, serves a dashboard and APIs to browsers, publishes to an MQTT broker, answers ASCOM Alpaca clients such as N.I.N.A., and sends alerts to push services and paired phones.
    I2C["<b>Sky and air</b><br/>TSL2591 brightness<br/>MLX90614 IR sky temperature<br/>BME280 temperature, humidity, pressure"]
    OPT["<b>Optional</b><br/>GPS: location and time<br/>RG-15: rain<br/>Anemometer and vane: wind"]
    ESP["<b>ESP32 running SQMeter</b><br/>readings, cloud cover,<br/>safety verdict, alerts<br/><i>settings in NVS,<br/>web UI in LittleFS</i>"]
    WEB["Browser<br/>dashboard, REST, WebSocket"]
    MQTT["MQTT broker<br/>Home Assistant discovery"]
    ALPACA["ASCOM Alpaca client<br/>e.g. N.I.N.A."]
    PUSH["Push alerts<br/>ntfy, Pushover, webhook"]
    PHONE["Phone over Bluetooth<br/>BLE build only"]
    NET["Internet<br/>NTP time, GitHub releases"]
    I2C -->|I²C| ESP
    OPT -->|UART, pulses, analogue| ESP
    NET -.->|time, updates| ESP
    ESP <--> WEB
    ESP <-->|readings, safety, alerts| MQTT
    ESP <--> ALPACA
    ESP --> PUSH
    ESP <--> PHONE
```
*Figure: What SQMeter connects to: sensors in, the ESP32 in the middle, and everything it talks to.*

<details><summary>Diagram in words</summary>

- **Sensors** into the ESP32: TSL2591 sky brightness, MLX90614 IR sky temperature and BME280 temperature, humidity and pressure on I²C; optional GPS and RG-15 rain gauge on serial (UART); optional anemometer (pulses) and wind vane (analogue).
- **The ESP32** works out the readings, cloud cover, the safety verdict and alerts, with settings in NVS and the web UI in LittleFS.
- **Outputs**: browsers (dashboard, REST, WebSocket); an MQTT broker (readings, safety, alerts, Home Assistant discovery, alerts pause/resume); ASCOM Alpaca clients such as N.I.N.A.; push alerts (ntfy, Pushover, webhook, MQTT); a phone over Bluetooth on the BLE build.
- **From the internet**: NTP time and GitHub releases for updates.

</details>

## Highlights

- TSL2591 light sensor — SQM, NELM, Bortle 1–9
- BME280 — temperature, humidity, pressure
- MLX90614 — IR cloud temperature and cloud cover estimate
- GPS support (optional) — location and precise time
- RG-15 rain sensor and anemometer / wind vane support (optional)
- Native ASCOM Alpaca SafetyMonitor + ObservingConditions (N.I.N.A.-compatible, no bridge), tested with ConformU on every PR
- Safety rules for rain, wind, cloud, SQM, humidity and dew point, with the reasons shown and a safe delay
- Alerts via Pushover, ntfy, webhook, MQTT or a paired phone over Bluetooth — per-event levels and sounds, your own wording, only when it's dark, sent any time or only while an imaging app is connected, a pause switch for Home Assistant, and an alert when the imaging app goes quiet.
- Real-time dashboard with reorderable cards and a Sun & Moon night chart
- REST API and MQTT publishing (including a 1/0 safe flag for logging)
- OTA firmware updates from the browser, or self-updated directly from GitHub Releases
- Captive portal Wi-Fi setup on first boot

## Quick Start

See [Flashing Your Device](https://sqmeter.dev/getting-started/flashing/) to get started with a new ESP32.

## Current Security Model

SQMeter is intended for a trusted local network. The current firmware has no web/API authentication, unauthenticated OTA endpoints, and exposes saved configuration through the LAN API. Do not port-forward the device or place it on guest WiFi.
