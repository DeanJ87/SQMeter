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

Then re-flash from the release `sqmv2-complete-flash-*.bin`.
