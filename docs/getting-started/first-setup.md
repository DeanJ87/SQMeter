# First Boot

After flashing, SQMv2 has no WiFi credentials. It will start a captive portal so you can configure it from any phone or laptop.

---

## Step 1 — Connect to the Setup AP

Power on the ESP32. Within a few seconds it will broadcast a WiFi network:

```
SSID: SQM-Setup
```

Connect to it with any device. No password required.

---

## Step 2 — Open the Captive Portal

Navigate to:

```
http://192.168.4.1
```

Most phones will show a "Sign in to network" prompt automatically and take you there.

---

## Step 3 — Configure WiFi

1. The portal will scan for nearby networks
2. Select your network and enter the password
3. Hit **Connect**
4. The device will restart and join your network

!!! note "2.4 GHz only"
    ESP32 does not support 5 GHz networks. Make sure you're selecting a 2.4 GHz SSID.

---

## Step 4 — Find the Device

After connecting, the device logs its IP to serial. If you don't have a serial monitor open, check your router's DHCP client list for a host named `sqm-esp32` (or whatever hostname you configured).

The web interface is at:

```
http://sqm-esp32.local   # mDNS — works on most networks
http://<device-ip>       # Direct IP — always works
```

---

## WiFi Config is Persistent

Credentials are stored in the NVS partition, separate from the firmware and filesystem. You can:

- Flash new firmware → WiFi stays configured
- Update the web UI filesystem → WiFi stays configured
- Power cycle the device → WiFi stays configured

The only way to clear it is a full chip erase:

```bash
esptool.py --chip esp32 --port PORT erase_flash
```
