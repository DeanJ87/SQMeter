# SQM v2 - Modern ESP32 Dark Sky Monitoring System

A sophisticated dark sky quality monitoring system built with ESP32, featuring real-time sensor data, web-based interface, and MQTT integration.

## Features

### Hardware & Sensors
- **TSL2591** - High-sensitivity light sensor for lux/SQM measurements
- **BME280** - Environmental sensor (temperature, humidity, pressure, dew point)
- **MLX90614** - IR thermometer for cloud detection (sky vs ambient temperature)
- **GPS** (optional) - Location and precise time synchronization
- ESP32 with WiFi connectivity

### Sky Quality Measurements
- **SQM** (Sky Quality Meter) - mag/arcsec²
- **NELM** (Naked Eye Limiting Magnitude)
- **Bortle Scale** - Dark sky classification (1-9)
- Lux to astronomical magnitude conversion

### Web Interface
- Modern single-page application (Preact + TypeScript)
- Real-time data via WebSocket (no polling)
- Responsive dark-themed UI with TailwindCSS
- Four main pages:
  - **Dashboard** - Live sensor readings and sky quality
  - **Settings** - Device configuration
  - **System** - Status and system controls
  - **Updates** - OTA firmware updates

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
- Strong typing (no stringly-typed code)
- RAII resource management
- Const correctness
- Comprehensive error handling
- Structured logging with levels
- Compilation with `-Wall -Wextra -Werror`
- Watchdog timer for crash recovery

## Project Structure

```
SQMv2/
├── platformio.ini
├── partitions.csv
├── config.example.json
├── scripts/
│   └── build_web.py
├── include/
│   ├── Config.h
│   ├── Logger.h
│   ├── WiFiManager.h
│   ├── WebServer.h
│   ├── MQTTClient.h
│   ├── TimeManager.h
│   ├── TCPServer.h
│   ├── sensors/
│   │   ├── SensorBase.h
│   │   ├── TSL2591Sensor.h
│   │   ├── BME280Sensor.h
│   │   ├── MLX90614Sensor.h
│   │   └── GPSSensor.h
│   └── calculations/
│       ├── SkyQuality.h
│       └── CloudDetection.h
├── src/
│   ├── main.cpp
│   ├── Config.cpp
│   ├── Logger.cpp
│   ├── WiFiManager.cpp
│   ├── WebServer.cpp
│   ├── MQTTClient.cpp
│   ├── TimeManager.cpp
│   ├── TCPServer.cpp
│   ├── sensors/
│   │   ├── TSL2591Sensor.cpp
│   │   ├── BME280Sensor.cpp
│   │   ├── MLX90614Sensor.cpp
│   │   └── GPSSensor.cpp
│   └── calculations/
│       ├── SkyQuality.cpp
│       └── CloudDetection.cpp
└── web/
    ├── package.json
    ├── package-lock.json
    ├── tsconfig.json
    ├── vite.config.ts
    ├── tailwind.config.js
    ├── postcss.config.js
    └── src/
        ├── main.tsx
        ├── App.tsx
        ├── index.css
        ├── components/
        │   ├── Dashboard.tsx
        │   ├── Settings.tsx
        │   ├── System.tsx
        │   ├── Layout.tsx
        │   └── Updates.tsx
        ├── hooks/
        │   └── useWebSocket.ts
        ├── types/
        │   └── index.ts
        ├── utils/
        │   └── timezone.ts
        └── validation/
            └── configSchema.ts
```

## Getting Started

### Prerequisites

- **PlatformIO** - Install via VS Code extension or CLI
- **Node.js 18+** - For building the web interface
- **Python 3** - For build scripts

### Hardware Connections

Default I2C pins (configurable in settings):
- **SDA** - GPIO 21
- **SCL** - GPIO 22

Both sensors use I2C interface:
- TSL2591 - Default address 0x29
- BME280 - Default address 0x76

### Build & Upload

1. **Clone the repository**
   ```bash
   cd SQMv2
   ```

2. **Build the firmware**
   ```bash
   pio run
   ```

3. **Build the web interface** (automatic during filesystem build)
   ```bash
   cd web
   npm install
   npm run build
   cd ..
   ```

4. **Upload firmware**
   ```bash
   pio run --target upload
   ```

5. **Upload filesystem** (includes web UI)
   ```bash
   pio run --target uploadfs
   ```

### First-Time Setup

1. Power on the ESP32
2. If no WiFi is configured, it will create an AP: `SQM-Setup`
3. Connect to the AP and navigate to `192.168.4.1`
4. Configure WiFi credentials via the web interface
5. Device will connect to your network

### Configuration

Configuration is stored in NVS (Non-Volatile Storage) on the device, which persists across firmware and filesystem updates. You can modify it via:
- Web interface Settings page
- Direct API calls to `/api/config`

Default configuration includes:
- WiFi settings (SSID, password, hostname)
- MQTT settings (broker, port, topic)
- Sensor settings (I2C pins, read interval)
- Device metadata (name, timezone)

## API Reference

### REST Endpoints

- `GET /api/status` - System status (uptime, memory, WiFi)
- `GET /api/sensors` - Current sensor readings
- `GET /api/config` - Get configuration
- `POST /api/config` - Update configuration
- `POST /api/restart` - Restart device
- `GET /api/wifi/scan` - Scan for WiFi networks
- `POST /api/wifi/connect` - Connect to WiFi
- `POST /api/update` - OTA firmware update

### WebSocket

Connect to `/ws/sensors` for real-time sensor updates:

```javascript
const ws = new WebSocket('ws://device-ip/ws/sensors');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Sensor data:', data);
};
```

Data format:
```json
{
  "lightSensor": {
    "lux": 0.000234,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "status": 0
  },
  "environment": {
    "temperature": 22.5,
    "humidity": 45.2,
    "pressure": 1013.25,
    "dewpoint": 11.3,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.50,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site"
  },
  "irTemperature": {
    "skyTemp": -18.5,
    "ambientTemp": 21.3,
    "status": 0
  },
  "cloudConditions": {
    "condition": 1,
    "description": "Clear",
    "temperatureDelta": -39.8,
    "correctedDelta": -40.2,
    "cloudCoverPercent": 0.0,
    "humidityUsed": 45.2
  }
}
```

## MQTT Integration

Enable MQTT in settings to publish sensor data to your broker:

**Topic structure:** `{configured-topic}`

**Payload format:**
```json
{
  "timestamp": 12345,
  "light": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45
  },
  "sky": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0
  },
  "environment": {
    "temperature": 22.5,
    "humidity": 45.2,
    "pressure": 1013.25
  }
}
```

**Note:** full payload structure mirrors the WebSocket format above.

## Development

### Web UI Development

```bash
cd web
npm install
npm run dev
```

The dev server will proxy API requests to your ESP32 device (configure in `vite.config.ts`).

### Building for Production

```bash
cd web
npm run build
```

Output goes to `web/dist/`, which is automatically copied to `data/` folder by the PlatformIO build script.

### Extending Sensors

Add new sensors by extending `SensorBase`:

```cpp
class NewSensor : public SensorBase {
public:
    bool begin() override;
    void update() override;
    std::string getName() const override;
    std::string toJson() const override;
};
```

## Sky Quality Calculations

### SQM (Sky Quality Meter)

Converts lux to magnitudes per square arcsecond:

```
SQM = -2.5 × log₁₀(lux) + 12.59
```

### NELM (Naked Eye Limiting Magnitude)

Estimated using empirical formulas based on SQM value and atmospheric conditions.

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

## Troubleshooting

### Sensors Not Detected

1. Check I2C wiring (SDA/SCL pins)
2. Verify I2C addresses with scanner
3. Check power supply (3.3V for sensors)
4. Review serial logs for error messages

### WiFi Connection Issues

1. Check credentials in settings
2. Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
3. Check signal strength
4. Use captive portal for reconfiguration

### Web UI Not Loading

1. Ensure filesystem was uploaded (`pio run --target uploadfs`)
2. Check that web files exist in `data/` folder
3. Rebuild web UI: `cd web && npm run build`
4. Verify LittleFS is mounted (check serial logs)

### OTA Update Fails

1. Ensure stable power supply
2. Check WiFi connection strength
3. Verify firmware file is valid `.bin`
4. Don't interrupt upload process

## Performance

- **Update Rate**: Configurable (default 5 seconds)
- **WebSocket Broadcast**: 1 second interval
- **Memory Usage**: ~100KB RAM typical
- **Flash Usage**: ~1.5MB firmware + ~200KB web UI
- **Power Consumption**: ~80mA @ 5V typical

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

**Enjoy your dark sky observations! 🌌✨**
