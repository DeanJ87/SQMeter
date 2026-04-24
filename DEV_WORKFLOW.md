# Development Workflow

## Config Storage - NVS (Non-Volatile Storage)

**IMPORTANT:** Configuration is now stored in NVS, not in the filesystem. This means:

✅ **You can freely upload the filesystem without losing WiFi config!**
✅ **No more captive portal loop during development**
✅ **Web UI updates don't reset settings**

## Quick Commands

### Backend (C++) Development
```bash
pio run --target upload
```
Changes to sensors, MQTT, WiFi manager, etc.

### Frontend Development

#### Option 1: Local Dev Server (Best for active development)
```bash
cd web
# Set your ESP32's IP address
ESP32_IP=192.168.1.128 npm run dev
```
Then open http://localhost:5173

**WebSocket will work because proxy is configured!**

#### Option 2: Build and Upload (Test on device)
```bash
cd web
npm run build
cd ..
pio run --target uploadfs   # Safe now - won't erase WiFi config!
```

## Current Setup

Your device is in **captive portal mode** at `192.168.4.1` because it has no WiFi configured in NVS yet.

### To Configure WiFi:
1. Connect to "SQM-Setup" WiFi network
2. Go to http://192.168.4.1
3. Select your network and enter password
4. It will connect and give you the new IP address
5. **Your WiFi config is now saved in NVS!**

### For Development After WiFi is Configured:

**Web Dev:**
```bash
cd web
ESP32_IP=<your-device-ip> npm run dev
```

**Firmware Dev:**
```bash
pio run --target upload  # Your WiFi config stays intact!
```

**Web UI Updates:**
```bash
cd web
npm run build
cd ..
pio run --target uploadfs  # Your WiFi config stays intact!
```

## NVS Storage Details

- **Location:** Separate NVS partition (20KB at 0x9000)
- **Namespace:** "sqm"
- **Key:** "config"
- **Survives:** Firmware uploads, filesystem uploads, power cycles
- **Cleared by:** Full chip erase only

To manually clear config (force captive portal):
```bash
pio run --target erase
pio run --target upload
pio run --target uploadfs
```

## WebSocket in Dev Mode

The `vite.config.ts` now has proper WebSocket proxy configuration:

```typescript
{
  target: `ws://${ESP32_IP}`,
  ws: true,
  changeOrigin: true,
}
```

Set `ESP32_IP` environment variable or update it in `vite.config.ts` (line 7).
