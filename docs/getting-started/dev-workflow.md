# Development Workflow

This guide is for contributors and developers working on SQMeter firmware or the web frontend. It covers the NVS-based config storage model and the recommended workflows for iterating quickly.

## Config Storage - NVS (Non-Volatile Storage)

!!! important
    Configuration is stored in NVS, not in the filesystem. This means:

    - You can freely upload the filesystem without losing WiFi config
    - No more captive portal loop during development
    - Web UI updates don't reset settings

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

WebSocket will work because a proxy is configured in `vite.config.ts`.

#### Option 2: Build and Upload (Test on device)

```bash
cd web
npm run build
cd ..
pio run --target uploadfs   # Safe - won't erase WiFi config
```

## Current Setup

Your device enters **captive portal mode** at `192.168.4.1` when no WiFi is configured in NVS.

### To Configure WiFi

1. Connect to "SQM-Setup" WiFi network
2. Go to http://192.168.4.1
3. Select your network and enter password
4. Device will connect and show the new IP address
5. WiFi config is now saved in NVS

### For Development After WiFi is Configured

**Web Dev:**
```bash
cd web
ESP32_IP=<your-device-ip> npm run dev
```

**Firmware Dev:**
```bash
pio run --target upload  # WiFi config stays intact
```

**Web UI Updates:**
```bash
cd web
npm run build
cd ..
pio run --target uploadfs  # WiFi config stays intact
```

## NVS Storage Details

| Property | Value |
|----------|-------|
| Location | Separate NVS partition (20KB at 0x9000) |
| Namespace | `sqm` |
| Key | `config` |
| Survives | Firmware uploads, filesystem uploads, power cycles |
| Cleared by | Full chip erase only |

To manually clear config and force captive portal:

```bash
pio run --target erase
pio run --target upload
pio run --target uploadfs
```

## WebSocket in Dev Mode

The `vite.config.ts` includes proper WebSocket proxy configuration:

```typescript
{
  target: `ws://${ESP32_IP}`,
  ws: true,
  changeOrigin: true,
}
```

Set `ESP32_IP` as an environment variable or update it directly in `vite.config.ts` (line 7).
