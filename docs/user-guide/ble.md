# Bluetooth (BLE)

The optional **BLE firmware build** broadcasts SQMeter's safety verdict and rain state over Bluetooth Low Energy, and serves a read-only GATT service with notifications. It's useful when there's no WiFi at the pier, or for a Bluetooth proxy (e.g. Home Assistant's) to pick up rain/safety without any network setup.

!!! warning "Separate firmware build"
    NimBLE adds ~235 KB, which doesn't fit the standard 1.5 MB app partitions. The BLE build (`esp32dev-ble`) uses a different partition layout (`partitions_ble.csv`, 1.69 MB app slots), so the **first install must be over USB** - OTA can't change the partition table. After that, GitHub updates on the Updates page automatically fetch the matching `sqmeter-ble-firmware-*` release asset. Your settings (NVS) are kept when switching builds.

!!! note "BLE slows WiFi down"
    The ESP32 has one 2.4 GHz radio, shared between WiFi and Bluetooth by time-slicing. With Bluetooth on, the web UI and Alpaca respond more slowly. Measured on an ESP32-D0WD-V3 over a distant access point (already ~240 ms average ping with BLE off): HTTP requests took 0.3-3.6 s with BLE on, against 65-200 ms with it off. On a strong WiFi link the absolute delay is smaller, but expect a noticeable slowdown. The firmware already advertises only every 0.5-1 s and gives WiFi priority. N.I.N.A. still works, but if you rely on fast Alpaca polling, leave Bluetooth off - it's a setting, so the BLE build with Bluetooth disabled behaves exactly like the standard build.

## Installing

```bash
pio run -e esp32dev-ble -t upload
pio run -e esp32dev-ble -t uploadfs
```

or flash `sqmeter-ble-complete-flash-<version>.bin` from a release at offset `0x0`.

Then turn on **Settings → Device → Bluetooth → Turn on Bluetooth**, save, and restart. Going back to the standard build is the same, using `-e esp32dev`.

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

The characteristics below are read + notify and need no pairing; the phone alarm characteristics (next section) do.

| Characteristic | UUID | Format | Notifies |
|---|---|---|---|
| Safety | `c5a10001-…` | `[safe u8][rawSafe u8][reasonFlags u32]` - reason bits as in [`/api/safety`](../api/rest.md#get-apisafety) | On change |
| Rain | `c5a10002-…` | `[flags u8][rate u16, 0.01 mm/h]` - flags as advertisement bits 1, 2, 4 | On change |
| Latest alert | `c5a10003-…` | JSON `{"event","title","message","level"}` | Each alert (when alerts are enabled) |
| Sensor summary | `c5a10004-…` | JSON `{"sqm","cloud","skyT","temp","hum","dew","press"}` (fields omitted when unavailable) | Every 30 s |

The `…` suffix is `-7d1e-4b8a-9f3c-2e5d6a7b8c90` throughout.

Up to two clients can be connected at once; the device keeps advertising while connected.

## Phone alarm (pairing required)

Set a 6-digit **pairing passkey** in **Settings → Device → Bluetooth** (or press Generate), save, and restart. The device then also offers three secured characteristics, used by a phone app to wake you up:

| Characteristic | UUID | Access | Format |
|---|---|---|---|
| Alarm | `c5a10005-…` | read + **indicate**, encrypted + authenticated | `[seq u32][level u8][reasonFlags u32][epoch u32]` |
| Ack | `c5a10006-…` | write, encrypted + authenticated | `[seq u32]` |
| Heartbeat | `c5a10007-…` | read + notify, encrypted + authenticated | `[seq u32][uptime seconds u32]`, every 60 s |

- **Pairing** uses LE Secure Connections with bonding and the static passkey (the device "displays" it via Settings; you type it on the phone). Bonds are stored on the device; **Unpair all phones** removes them. Links that aren't paired this way never receive alarm or heartbeat data - the device doesn't send them to unencrypted connections.
- **Levels:** `2` = alarm (wake someone), `1` = information (e.g. safe again, rain cleared), `0` = nothing active / acknowledged.
- **Which events alarm** is set on the Alerts tab: every event whose level is **Wake me** rings paired phones (by default rain starting and a sensor failing). Phones ring even when **Send alerts** is off (no push channels), but not while alerts are switched off with **Alerts on now** / the bell - that's the "not imaging" switch, and it silences everything.
- **An alarm repeats** (re-indicated every 30 s) **until a phone writes its `seq` to Ack** - even if the condition clears meanwhile, since someone should still know it rained with the roof open. Information events never overwrite an active alarm. An ack with an old `seq` is ignored, so it can't cancel a newer alarm. After an ack the alarm is re-sent with level `0`, so every paired phone stops ringing.
- An alarm can also be acknowledged from **Settings → Device → Bluetooth**. Acknowledgements appear in the recent alerts and are sent to the alert channels when alerts are on.
- **Heartbeat:** if a phone hears nothing for a few minutes, the link or the device is down - an app should alarm on that too.

`GET /api/status` reports `ble.alarm` (`serviceActive`, `active`, `sequence`, `acknowledgedSequence`, `bondedPhones`). `POST /api/ble/ack` acknowledges the active alarm and `POST /api/ble/forget-bonds` unpairs all phones (both require HTTP auth when it's enabled).
