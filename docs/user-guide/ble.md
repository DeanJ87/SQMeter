# Bluetooth (BLE)

The optional **BLE firmware build** broadcasts SQMeter's safety verdict and rain state over Bluetooth Low Energy, and serves a read-only GATT service with notifications. It's useful when there's no WiFi at the pier, or for a Bluetooth proxy (e.g. Home Assistant's) to pick up rain/safety without any network setup.

!!! warning "Separate firmware build"
    NimBLE adds ~235 KB, which doesn't fit the standard 1.5 MB app partitions. The BLE build (`esp32dev-ble`) uses a different partition layout (`partitions_ble.csv`, 1.69 MB app slots), so the **first install must be over USB** - OTA can't change the partition table. After that, GitHub updates on the Updates page automatically fetch the matching `sqmeter-ble-firmware-*` release asset. Your settings (NVS) are kept when switching builds.

!!! note "BLE slows WiFi down"
    The ESP32 has one 2.4 GHz radio, shared between WiFi and Bluetooth by time-slicing. With Bluetooth on, the web UI and Alpaca respond more slowly. Measured on an ESP32-D0WD-V3: HTTP requests took 0.3-3.6 s with BLE on, against 65-200 ms with it off. The firmware already advertises only every 0.5-1 s and gives WiFi priority. N.I.N.A. still works, but if you rely on fast Alpaca polling, leave Bluetooth off - it's a setting, so the BLE build with Bluetooth disabled behaves exactly like the standard build.

## Installing

```bash
pio run -e esp32dev-ble -t upload
pio run -e esp32dev-ble -t uploadfs
```

or flash `sqmeter-ble-complete-flash-<version>.bin` from a release at offset `0x0`.

Then enable **Settings → Bluetooth (BLE) → Enable Bluetooth**, save, and restart. Going back to the standard build is the same, using `-e esp32dev`.

## What a phone sees

There's no SQMeter phone app yet. Any generic BLE explorer (nRF Connect, LightBlue) can connect, read, and subscribe to notifications. A dedicated app or Home Assistant integration can build on the layout below.

### Advertisement

The device advertises under its device name with the SQMeter service UUID, and manufacturer-specific data (company ID `0xFFFF`, little-endian):

| Bytes | Content |
|---|---|
| 0-1 | `0xFFFF` company ID |
| 2-3 | `"SQ"` magic |
| 4 | Format version (`1`) |
| 5 | Flags: bit0 safe, bit1 raining, bit2 rain sensor enabled, bit3 safety evaluated, bit4 rain sensor healthy |
| 6-7 | SQM × 100 (`0xFFFF` = unavailable) |

The flags are updated within a second of a change, so a scanner can watch safe/rain state without connecting.

### GATT service `c5a10000-7d1e-4b8a-9f3c-2e5d6a7b8c90`

All characteristics are read + notify; there are no writable characteristics, so no pairing is required.

| Characteristic | UUID | Format | Notifies |
|---|---|---|---|
| Safety | `c5a10001-…` | `[isSafe u8][rawSafe u8][reasonFlags u32]` - reason bits as in [`/api/safety`](../api/rest.md#get-apisafety) | On change |
| Rain | `c5a10002-…` | `[flags u8][rate u16, 0.01 mm/h]` - flags as advertisement bits 1, 2, 4 | On change |
| Latest alert | `c5a10003-…` | JSON `{"event","title","message","priority"}` | Each alert (when alerts are enabled) |
| Sensor summary | `c5a10004-…` | JSON `{"sqm","cloud","skyT","temp","hum","dew","press"}` (fields omitted when unavailable) | Every 30 s |

The `…` suffix is `-7d1e-4b8a-9f3c-2e5d6a7b8c90` throughout.

Up to two clients can be connected at once; the device keeps advertising while connected.
