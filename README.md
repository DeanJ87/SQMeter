# SQMeter - ESP32 Dark Sky Monitoring System

A sophisticated dark sky quality monitoring system built with ESP32, featuring real-time sensor data, web-based interface, and MQTT integration.

## Documentation

Full documentation is available in the [`docs/`](docs/) directory. To serve locally:

```bash
pip install -r requirements.txt
mkdocs serve
```

## Features

### Hardware & Sensors
- **TSL2591** - High-sensitivity light sensor for lux measurements
- **BME280** - Environmental sensor (temperature, humidity, pressure)
- **MLX90614** - IR temperature sensor for cloud detection
- ESP32 with WiFi connectivity and optional GPS

### Sky Quality Measurements
- **SQM** (Sky Quality Meter) - mag/arcsec²
- **NELM** (Naked Eye Limiting Magnitude)
- **Bortle Scale** - Dark sky classification (1-9)
- Cloud detection via IR temperature differential

### Web Interface
- Modern single-page application (Preact + TypeScript)
- Real-time data via WebSocket (no polling)
- Responsive dark-themed UI with TailwindCSS
- Dashboard, Settings, and System pages

### Backend Architecture
- **Async HTTP Server** on port 80
- **REST API** at `/api/*` endpoints
- **WebSocket** at `/ws/sensors` for real-time updates
- **OTA Updates** via web interface
- **LittleFS** for configuration and web files
- **MQTT** publishing (optional)
- **Captive Portal** for easy WiFi setup

### Code Quality
- C++17 with modern practices
- Strong typing, RAII resource management, const correctness
- Comprehensive error handling and structured logging
- Compilation with `-Wall -Wextra -Werror`
- Watchdog timer for crash recovery

## Project Structure

```
SQMeter/
├── platformio.ini          # PlatformIO configuration
├── partitions.csv          # ESP32 partition table
├── requirements.txt        # MkDocs dependencies
├── docs/                   # MkDocs documentation
│   ├── index.md
│   ├── getting-started/
│   │   ├── build.md
│   │   └── dev-workflow.md
│   └── reference/
│       ├── api.md
│       ├── configuration.md
│       └── hardware.md
├── scripts/
│   └── build_web.py        # Automated web UI build script
├── include/                # C++ headers
├── src/                    # C++ implementation
└── web/                    # Frontend source
```

## Getting Started

### Prerequisites

- **PlatformIO** - Install via VS Code extension or CLI
- **Node.js 18+** - For building the web interface
- **Python 3** - For build scripts

### Quick Build & Upload

```bash
# Build firmware
pio run

# Build web interface
cd web && npm install && npm run build && cd ..

# Upload firmware
pio run --target upload

# Upload filesystem (includes web UI)
pio run --target uploadfs
```

### First-Time Setup

1. Power on the ESP32
2. If no WiFi is configured, it will create an AP: `SQM-Setup`
3. Connect to the AP and navigate to `192.168.4.1`
4. Configure WiFi credentials via the web interface
5. Device will connect to your network

For detailed build options, OTA updates, and troubleshooting see [docs/getting-started/build.md](docs/getting-started/build.md).

## Sky Quality Calculations

### SQM (Sky Quality Meter)

Converts lux to magnitudes per square arcsecond:

```
SQM = -2.5 × log₁₀(lux) + 12.59
```

### Bortle Dark Sky Scale

| Class | SQM Range | Description |
|-------|-----------|-------------|
| 1 | >21.99 | Excellent dark-sky site |
| 2 | 21.89-21.99 | Typical truly dark site |
| 3 | 21.69-21.89 | Rural sky |
| 4 | 20.49-21.69 | Rural/suburban transition |
| 5 | 19.50-20.49 | Suburban sky |
| 6 | 18.94-19.50 | Bright suburban sky |
| 7 | 18.38-18.94 | Suburban/urban transition |
| 8 | 17.00-18.38 | City sky |
| 9 | <17.00 | Inner-city sky |

## License

This project is provided as-is for educational and hobby purposes.

## Credits

Built with:
- ESP-IDF framework
- Adafruit sensor libraries
- ArduinoJson
- AsyncWebServer
- Preact
- TypeScript
- TailwindCSS
- Vite

## Contributing

Feel free to submit issues and pull requests for:
- Bug fixes
- New sensor support
- UI improvements
- Documentation updates

---

**Enjoy your dark sky observations!**
