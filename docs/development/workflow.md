# Dev Workflow

## Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or VS Code with PlatformIO extension
- Node.js 18+
- Python 3

---

## Key Facts About Storage

Config lives in **NVS** (Non-Volatile Storage) — a separate 20 KB partition at `0x9000`. This means:

!!! success "Safe to do freely"
    - `pio run --target upload` — reflash firmware, WiFi config untouched
    - `pio run --target uploadfs` — update web UI, WiFi config untouched

!!! danger "Clears everything"
    `pio run --target erase` followed by re-flash — only do this to force the captive portal

---

## Firmware Development (C++)

```bash
# Build
pio run

# Build + upload
pio run --target upload

# Serial monitor
pio device monitor
```

Source lives in `src/` and `include/`. PlatformIO handles the toolchain.

---

## Frontend Development (TypeScript / Preact)

### Option A — Live dev server (recommended)

Proxies API and WebSocket calls to your real device:

```bash
cd web
ESP32_IP=192.168.1.42 npm run dev
```

Open `http://localhost:5173`. Changes hot-reload instantly. WebSocket works through the Vite proxy.

### Option B — Build and flash

```bash
cd web
npm run build
cd ..
pio run --target uploadfs
```

---

## Full Build (CI equivalent)

```bash
cd web && npm install && npm run build && cd ..
cp -r web/dist data
pio run                  # firmware
pio run --target buildfs # littlefs image
```

---

## NVS Details

| Property | Value |
|----------|-------|
| Partition | `nvs` at `0x9000` |
| Namespace | `sqm` |
| Key | `config` |
| Survives | Firmware uploads, filesystem uploads, power cycles |
| Cleared by | Full chip erase only |

To force the captive portal (reset all config):

```bash
esptool.py --chip esp32 --port PORT erase_flash
pio run --target upload
pio run --target uploadfs
```
