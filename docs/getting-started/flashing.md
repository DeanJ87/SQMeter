# Flashing Your Device

Download the latest release from [GitHub Releases](https://github.com/DeanJ87/SQMeter/releases) and flash your ESP32 over USB. You only need to do this once — after that you can update over WiFi.

---

## What's in a Release

Each release ships three binaries:

| File | When to use |
|------|-------------|
| `sqmeter-complete-flash-vX.Y.Z.bin` | **Fresh ESP32** — everything in one file |
| `sqmeter-firmware-vX.Y.Z.bin` | OTA update via web UI |
| `sqmeter-littlefs-vX.Y.Z.bin` | Web UI update only |

For a brand-new device, you only need `sqmeter-complete-flash-*.bin`.

---

## Prerequisites

Install `esptool`:

```bash
pip install esptool
```

---

## Fresh Flash (Recommended for New Devices)

One command, one file. Flashes bootloader, partition table, firmware, and web UI filesystem all at once:

```bash
esptool.py --chip esp32 --port PORT --baud 115200 \
  write_flash 0x0 sqmeter-complete-flash-v0.0.1.bin
```

Replace `PORT` with your serial port:

=== "macOS / Linux"
    ```
    /dev/cu.usbserial-*   # macOS
    /dev/ttyUSB0          # Linux
    ```

=== "Windows"
    ```
    COM3
    ```

!!! tip "Finding your port"
    ```bash
    esptool.py --chip esp32 chip_id
    ```
    esptool will scan and print the detected port.

---

## Selective Updates (Existing Devices)

Prefer OTA updates from the web UI — no cable needed. But if you need USB:

### Firmware only
```bash
esptool.py --chip esp32 --port PORT --baud 115200 \
  write_flash 0x10000 sqmeter-firmware-v0.0.1.bin
```

### Web UI filesystem only
```bash
esptool.py --chip esp32 --port PORT --baud 115200 \
  write_flash 0x310000 sqmeter-littlefs-v0.0.1.bin
```

!!! note "NVS is safe"
    Firmware and filesystem updates never touch the NVS partition where your WiFi credentials and settings are stored.

---

## Flash Memory Map

| Partition | Offset | Size | Contents |
|-----------|--------|------|----------|
| nvs | `0x9000` | 20 KB | WiFi credentials, settings |
| otadata | `0xe000` | 8 KB | OTA boot slot indicator |
| app0 | `0x10000` | 1.5 MB | Firmware |
| app1 | `0x190000` | 1.5 MB | OTA update slot |
| spiffs | `0x310000` | 512 KB | Web UI (LittleFS) |
