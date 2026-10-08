# Troubleshooting

---

## Sensors Not Detected

**Symptoms:** Dashboard shows `--` for all values. Serial log shows `init failed`.

1. Check I2C wiring — SDA/SCL can't be swapped
2. Confirm 3.3V on sensor VCC (not 5V)
3. Run an I2C scanner sketch to verify addresses
4. Check `i2cSDA` and `i2cSCL` in config match your wiring

---

## WiFi Won't Connect

1. Confirm the SSID is 2.4 GHz — ESP32 doesn't support 5 GHz
2. Check password (case-sensitive)
3. Move the device closer to the router for initial setup
4. If captive portal never appears, do a full erase and re-flash

---

## Web UI Not Loading

**`ERR_CONNECTION_REFUSED` or blank page:**

1. Verify the filesystem was flashed — `pio run --target uploadfs`
2. Check serial logs for `LittleFS mount failed`
3. Re-upload the filesystem: `pio run --target uploadfs`
4. If still broken, full erase and re-flash

---

## OTA Update Fails

1. Ensure stable power — don't run on a weak USB charger during flash
2. Check WiFi signal strength (RSSI better than -75 dBm)
3. Verify the `.bin` file is the right type (`firmware`, not `complete-flash` or `littlefs`)
4. Don't navigate away from the update page during upload

If the device stops responding after a failed OTA, it should fall back to the previous app slot on next boot. If it doesn't, reflash via USB.

"Could not activate partition" after an upload usually means the transfer was corrupted on a weak link - upload again.

---

## N.I.N.A. Can't See the Device

1. **Settings → Safety → ASCOM Alpaca** must be on.
2. Discovery is a UDP broadcast, which doesn't cross subnets or VLANs. If N.I.N.A.'s PC is on another network, add the device manually: its IP, port `80`.
3. After updating from a release before v0.2.0, re-select the devices in N.I.N.A. once - their unique IDs now include the MAC address.
4. Test from the PC: `curl http://<device-ip>/management/v1/configureddevices` should list both devices.

---

## An Alert Didn't Arrive (or One Did That Shouldn't Have)

Open **History** on the dashboard's Safety card. It lists safety changes, restarts (and why) and every safe/unsafe alert actually sent, and survives restarts. Common reasons for no alert:

- Alerts are switched off (the bell in the header is crossed out) or **Send alerts** is off
- It isn't dark yet and **Safety alerts only when it's dark** is on
- The device restarted - start-up and the safe delay aren't announced, see [Alerts](../user-guide/alerts.md#restarts)
- The cooldown held it back; it's sent when the cooldown ends if things haven't changed back
- The channel failed - the bell's list shows each channel's result, e.g. Pushover "user identifier is not a valid user" means the user key (not the app token) is wrong

---

## Web UI Slow With Bluetooth On

WiFi and Bluetooth share one radio. Expect slower page loads with Bluetooth on; turn it off in **Settings → Device** if you don't use the phone alarm.

---

## MQTT Not Publishing

1. Check `mqtt.enabled` is `true` in config
2. Ping the broker from another device to confirm it's reachable
3. Verify broker address and port
4. Check username/password if your broker requires auth
5. Watch broker logs: `mosquitto_sub -h broker -t "#" -v`

---

## Serial Monitor Garbled

Baud rate must be `115200`. In PlatformIO:

```bash
pio device monitor --baud 115200
```

---

## Resetting Everything

Nuclear option — clears firmware, filesystem, and NVS (WiFi config, all settings):

```bash
esptool.py --chip esp32 --port PORT erase_flash
```

Then re-flash from the release `sqmeter-complete-flash-*.bin`.
