# Flashing Your Device

Flash your ESP32 over USB once; after that it updates over WiFi from its Updates page.

The quickest way is the [browser flasher](usb-flash.md): plug the ESP32 in, pick the build and click Install. This page covers the same thing with `esptool` on the command line.

!!! note "Moving from v0.2 to v0.3 or later"
    v0.3 uses a new partition layout (more room for firmware), so a device on v0.2 moves to it with one USB flash. Your settings, WiFi included, are kept. See [One-time USB flash](usb-flash.md).

---

## What's in a Release

| File | What it's for |
|------|---------------|
| `sqmeter-l2-usb-standard-vX.Y.Z.zip` | **USB flash, standard build**: every part plus the `esptool` command |
| `sqmeter-l2-usb-ble-vX.Y.Z.zip` | **USB flash, Bluetooth build** |
| `sqmeter-l2-firmware-vX.Y.Z.bin` | Over-the-air update (standard build) |
| `sqmeter-l2-ble-firmware-vX.Y.Z.bin` | Over-the-air update (Bluetooth build) |
| `sqmeter-l2-littlefs-vX.Y.Z.bin` | The web UI, for both builds |
| `sqmeter-i18n-*` | Language files the device downloads |

`l2` is the partition layout the files are for. The device refuses an update file made for a different layout or the other build.

### Standard or Bluetooth build?

| Build | Choose it if |
|---|---|
| Standard | You don't need Bluetooth - recommended |
| Bluetooth | You want the phone alarm or BLE broadcasts ([Bluetooth](../user-guide/ble.md)) |

Both builds use the same partition layout, so you can switch between them with a USB flash and keep your settings.

---

## Flash with esptool

Install `esptool`:

```bash
pip install esptool
```

Unzip the USB package for your build and, from that folder:

```bash
esptool.py --chip esp32 --port PORT --baud 460800 write_flash \
  0x1000 bootloader.bin 0x8000 partitions.bin 0xE000 boot_app0.bin \
  0x10000 firmware.bin 0x390000 littlefs.bin
```

The same command is in the package's `FLASH.txt`. Replace `PORT` with your serial port:

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

!!! note "Settings are kept"
    Nothing is written between `0x9000` and `0xDFFF`, where the device keeps its settings and WiFi
    credentials. Don't run `erase_flash` unless you want to start again from the setup hotspot.

---

## Flash Memory Map

The same for both builds:

| Partition | Offset | Size | Contents |
|-----------|--------|------|----------|
| nvs | `0x9000` | 20 KB | Settings, WiFi credentials |
| otadata | `0xE000` | 8 KB | Which app slot boots |
| app0 | `0x10000` | 1.75 MB | Firmware |
| app1 | `0x1D0000` | 1.75 MB | The other firmware slot (over-the-air updates) |
| spiffs | `0x390000` | 448 KB | Web UI and language file (LittleFS) |
