---
title: Open-source sky quality meter for ESP32
description: "Open-source ESP32 sky quality meter and observatory safety monitor: SQM, Bortle, cloud and rain, native ASCOM Alpaca for N.I.N.A., alerts and MQTT."
---

# SQMeter

<div class="sqm-hero" markdown>
<div class="sqm-hero-value">21.47</div>
<div class="sqm-hero-unit">mag / arcsec² · Bortle 2 · Typical truly dark site</div>

ESP32 dark sky quality monitor - sky brightness, cloud, rain and wind, an observatory safety monitor for N.I.N.A., and alerts that wake you when it matters.
</div>

SQMeter is open-source (MIT) firmware that makes an ESP32 and a few I²C sensors into a sky quality meter and observatory safety monitor. It measures sky brightness (SQM in mag/arcsec², Bortle class, NELM), cloud cover, rain and wind, and is a native ASCOM Alpaca SafetyMonitor and ObservingConditions device for N.I.N.A. and other imaging apps. Everything is in a dashboard the device serves to any browser on your network. SQMeter is in public beta (v0.3.x).

[Try the Live Demo :material-arrow-right:](https://demo.sqmeter.dev/){ .md-button .md-button--primary }
[View on GitHub :fontawesome-brands-github:](https://github.com/DeanJ87/SQMeter){ .md-button }

<!-- diagram: DIA-01
sources: src/main.cpp#setup include/sensors/ include/WebServer.h include/MQTTClient.h include/AlertDispatcher.h include/BleService.h include/OtaUpdater.h include/TimeManager.h
blocking: false
fingerprint: c5469a7836233065
-->
<figure class="diagram" markdown>

```mermaid
flowchart LR
    accTitle: What SQMeter connects to
    accDescr: Sensors feed the ESP32. It keeps its settings and web UI on the device, serves a dashboard and APIs to browsers, publishes to an MQTT broker, answers ASCOM Alpaca clients such as N.I.N.A., and sends alerts to push services and paired phones.
    I2C["<b>Sky and air</b><br/>TSL2591 brightness, SQM<br/>optional second TSL2591, SQM-L, on I²C-2<br/>MLX90614 IR sky temperature<br/>BME280 temperature, humidity, pressure"]
    OPT["<b>Optional</b><br/>GPS: location and time<br/>RG-15: rain<br/>Anemometer and vane: wind<br/>OLED display, Li-ion battery gauge:<br/>ESP32-S3 boards"]
    ESP["<b>ESP32 running SQMeter</b><br/>readings, cloud cover,<br/>safety verdict, alerts<br/><i>settings in NVS,<br/>web UI and language file in LittleFS</i>"]
    WEB["Browser<br/>dashboard, REST, WebSocket"]
    MQTT["MQTT broker<br/>Home Assistant discovery"]
    ALPACA["ASCOM Alpaca client<br/>e.g. N.I.N.A."]
    PUSH["Push alerts<br/>ntfy, Pushover, webhook"]
    PHONE["Phone over Bluetooth<br/>BLE build only"]
    NET["Internet<br/>NTP time, GitHub releases<br/>(updates, language files)"]
    I2C -->|I²C| ESP
    OPT -->|UART, pulses, analogue| ESP
    NET -.->|time, updates, languages| ESP
    ESP <--> WEB
    ESP <-->|readings, safety, alerts| MQTT
    ESP <--> ALPACA
    ESP --> PUSH
    ESP <--> PHONE
```

<figcaption>What SQMeter connects to: sensors in, the ESP32 in the middle, and everything it talks to.</figcaption>
</figure>

??? info "Diagram in words"

    - **Sensors** (into the ESP32):
        - TSL2591 sky brightness, MLX90614 IR sky temperature and BME280 air temperature, humidity and pressure, on the I²C bus.
        - Optional GPS (location and time) and RG-15 rain gauge on serial (UART).
        - Optional anemometer (pulses) and wind vane (analogue).
        - On the ESP32-S3 boards: an optional OLED display and Li-ion battery fuel gauge on I²C.
    - **The ESP32** works out the readings, cloud cover, the safety verdict and alerts. It keeps its settings in NVS and the web UI's files in LittleFS.
    - **Outputs:**
        - Browsers get the dashboard, the REST API and live WebSocket updates.
        - An MQTT broker gets readings, the safety verdict and alerts, with Home Assistant discovery; it can also switch alerts on and off.
        - ASCOM Alpaca clients such as N.I.N.A. read the SafetyMonitor and ObservingConditions devices.
        - Alerts go to ntfy, Pushover, a webhook or MQTT.
        - On the Bluetooth (BLE) build, a paired phone gets the safety state and alarms, and can acknowledge them.
    - **From the internet:** NTP time and, when you ask for an update, GitHub releases.

![The SQMeter dashboard](assets/screenshots/dashboard.png)

---

## Features

<div class="grid cards" markdown>

- :material-telescope: **Sky Quality Measurements**

    SQM (mag/arcsec²), NELM, and Bortle Scale 1–9 calculated from raw lux.

- :material-wifi: **Web Interface**

    Real-time dashboard over WebSocket. No polling. No app required.

- :material-thermometer: **Environmental Sensors**

    Temperature, humidity, and pressure via BME280.

- :material-broadcast: **MQTT Publishing**

    Push readings to Home Assistant, Grafana, or any MQTT broker.

- :material-update: **OTA Updates**

    Flash new firmware from the browser, or let the device update itself directly from GitHub Releases.

- :material-connection: **ASCOM Alpaca**

    Native SafetyMonitor + ObservingConditions device for N.I.N.A. — no separate bridge required. Tested with ASCOM ConformU on every change.

- :material-shield-check: **Safety & alerts**

    Rain, wind, cloud, SQM, humidity and dew-point rules. Alerts via Pushover, ntfy, webhook, MQTT or a paired phone over Bluetooth — sent any time, or only while an imaging app is connected, and you're told if it goes quiet.

- :material-weather-windy: **Rain & wind**

    Optional Hydreon RG-15 rain gauge and anemometer / wind vane.

- :material-weather-night: **Sun & Moon**

    Twilight, darkness and moon chart for the night ahead from your location.

- :material-lock-open: **Open Hardware**

    Standard I2C sensors. Runs on any ESP32 dev board.

</div>

---

## Sensors

| Sensor | Measures | Interface |
|--------|----------|-----------|
| TSL2591 | Lux (full / visible / IR) | I2C `0x29` |
| BME280 | Temperature, humidity, pressure | I2C `0x76` |
| MLX90614 | IR cloud temperature | I2C `0x5A` |
| GPS (optional) | Location & time | UART |
| RG-15 (optional) | Rain detection | UART |

---

## FAQ

### Is SQMeter an SQM-LE/LU replacement?

Not a drop-in one. It reports the same quantity (sky brightness in mag/arcsec²), but it doesn't speak the Unihedron SQM-LE/LU protocol, so software written for those meters can't read it directly. Its readings come over the REST API, WebSocket, MQTT, ASCOM Alpaca (`skyquality`) and Bluetooth. A bare TSL2591 also sees a much wider patch of sky than a commercial SQM; fit a lens or baffle and use the SQM offset to match a reference meter ([Sky Quality Calculations](reference/sky-quality.md)).

### Does it work with N.I.N.A.?

Yes. The device is a native ASCOM Alpaca SafetyMonitor (interface v3) and ObservingConditions (interface v2), so N.I.N.A. finds it with Alpaca discovery: no Windows driver, no bridge. Both devices pass ASCOM ConformU on every change. Alpaca is off until you turn on **Serve Alpaca devices** in **Settings → Safety**; see [ASCOM Alpaca](user-guide/alpaca.md).

### What sensor does it use?

A TSL2591 light sensor for sky brightness (not the TSL237 in the Unihedron SQM), an MLX90614 infrared thermometer for sky temperature and cloud cover, and a BME280 for temperature, humidity and pressure. A Hydreon RG-15 rain gauge, an anemometer and wind vane, and GPS are optional. See [Hardware Setup](getting-started/hardware.md).

### Does it need Home Assistant or MQTT?

No. The device works on its own from the browser. MQTT, with Home Assistant discovery, is one of several optional outputs ([MQTT Integration](user-guide/mqtt.md)).

---

## Quick Start

New device? Go to [Flashing Your Device](getting-started/flashing.md) — you only need a USB cable and the release binaries.

Already flashed? Go to [First Boot](getting-started/first-setup.md) to configure WiFi.
