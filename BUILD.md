# SQMeter - Build and Upload Guide

## Quick Start

### 1. Install Dependencies

**PlatformIO:**
```bash
# Via VS Code
# Install "PlatformIO IDE" extension

# Or via CLI
pip install platformio
```

**Node.js:**
```bash
# macOS
brew install node

# Or download from https://nodejs.org/
```

### 2. Build Firmware

```bash
# Build firmware only
pio run

# Build with verbose output
pio run -v
```

### 3. Build Web Interface

```bash
cd web
npm install
npm run build
cd ..
```

The build script in `scripts/build_web.py` will automatically build the web UI before creating the filesystem image.

### 4. Upload to ESP32

**Upload firmware only (for code changes):**
```bash
pio run --target upload
```

**Upload filesystem (for web UI HTML/CSS changes):**
```bash
pio run --target uploadfs
```

**First-time setup (upload both):**
```bash
pio run --target uploadfs && pio run --target upload
```

**Note:** Configuration (WiFi credentials, MQTT settings, etc.) is stored in NVS (Non-Volatile Storage), which is separate from the filesystem. This means you can safely upload the filesystem without losing your WiFi configuration!

### 5. Monitor Serial Output

```bash
pio device monitor
```

Or use the PlatformIO IDE serial monitor.

## Development Workflow

### Backend Development (C++)

1. Make changes to C++ files in `src/` or `include/`
2. Build: `pio run`
3. Upload: `pio run --target upload`
4. Monitor: `pio device monitor`

### Frontend Development (TypeScript/Preact)

1. Start dev server:
   ```bash
   cd web
   # Set ESP32 IP address (or use default 192.168.1.128)
   ESP32_IP=192.168.1.xxx npm run dev
   ```
2. Open http://localhost:5173
3. Edit files in `web/src/`
4. Changes auto-reload in browser

**Note:** In dev mode, API and WebSocket calls are proxied to the ESP32 device. Set the `ESP32_IP` environment variable to your device's IP address, or update it in `vite.config.ts`.

### Production Build

1. Build web UI:
   ```bash
   cd web
   npm run build
   ```
2. Upload filesystem: `pio run --target uploadfs`
   - This only updates the web UI files
   - Your WiFi config and settings are stored in NVS and won't be affected
   - For firmware-only updates, use `pio run --target upload`

## OTA Updates

Update firmware over WiFi without USB cable.

### Via Web Interface

1. Build firmware: `pio run`
2. Locate `.pio/build/esp32dev/firmware.bin`
3. Navigate to System page in web UI
4. Upload firmware file
5. Device will restart automatically

### Via PlatformIO

```bash
# Configure in platformio.ini:
# upload_protocol = espota
# upload_port = <device-ip>

pio run --target upload
```

## Partition Table

Custom partition scheme (`partitions.csv`):

| Name | Type | SubType | Offset | Size |
|------|------|---------|--------|------|
| nvs | data | nvs | 0x9000 | 20KB |
| otadata | data | ota | 0xe000 | 8KB |
| app0 | app | ota_0 | 0x10000 | 1920KB |
| app1 | app | ota_1 | 0x1F0000 | 1920KB |
| littlefs | data | littlefs | 0x3D0000 | 192KB |

This allows:
- OTA updates with two app partitions
- 192KB for web UI and configuration
- NVS for system data

## Troubleshooting

### Build Errors

**Missing libraries:**
```bash
pio lib install
```

**Clean build:**
```bash
pio run --target clean
pio run
```

### Upload Errors

**Port not found:**
```bash
# List devices
pio device list

# Specify port manually
pio run --target upload --upload-port /dev/cu.usbserial-*
```

**Permission denied (macOS/Linux):**
```bash
sudo chmod 666 /dev/ttyUSB0
# or
sudo usermod -a -G dialout $USER
```

### Web UI Issues

**Build fails:**
```bash
cd web
rm -rf node_modules package-lock.json
npm install
npm run build
```

**Hot reload not working:**
- Check proxy configuration in `vite.config.ts`
- Verify ESP32 is accessible at configured IP
- Check browser console for errors

### Filesystem Upload Fails

**Insufficient space:**
- Reduce web UI size (remove unused assets)
- Optimize images and fonts
- Check partition table sizes

**LittleFS mount fails:**
- Re-upload filesystem: `pio run --target uploadfs`
- Check serial logs for errors
- Verify partition table is correct

## Production Deployment

1. **Set release configuration:**
   - Disable debug logging
   - Enable compiler optimizations
   - Set appropriate log levels

2. **Build optimized web UI:**
   ```bash
   cd web
   npm run build
   ```

3. **Flash complete system:**
   ```bash
   pio run --target upload
   pio run --target uploadfs
   ```

4. **Configure device:**
   - Access web interface
   - Set WiFi credentials
   - Configure MQTT (if needed)
   - Set device name and timezone

5. **Test thoroughly:**
   - Verify sensor readings
   - Check WebSocket connection
   - Test OTA updates
   - Confirm MQTT publishing

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      - uses: actions/setup-node@v3
        with:
          node-version: '18'
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build Web UI
        run: cd web && npm install && npm run build
      - name: Build Firmware
        run: pio run
```

## Performance Optimization

### Firmware Size

- Use `-Os` optimization level (already configured)
- Remove unused libraries
- Enable LTO (Link Time Optimization)

### Memory Usage

- Use `const` for read-only data (stored in flash)
- Minimize string allocations
- Use stack allocation when possible
- Monitor heap fragmentation

### Web UI Size

- Minimize bundle size
- Remove unused CSS (Tailwind purge)
- Compress assets
- Use appropriate image formats

## Backup & Restore

### Backup Configuration

```bash
# Via web interface
curl http://device-ip/api/config > config-backup.json

# Or download via browser
```

### Restore Configuration

```bash
# Via web interface
curl -X POST http://device-ip/api/config \
  -H "Content-Type: application/json" \
  -d @config-backup.json

# Or use the web UI Settings page
```

### Complete Backup

1. Download configuration
2. Save firmware: `.pio/build/esp32dev/firmware.bin`
3. Save filesystem image: `.pio/build/esp32dev/spiffs.bin`

---

Happy building! 🚀
