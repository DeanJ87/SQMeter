# First Boot

SQMeter ships with no WiFi credentials. On first power-on it starts in **hotspot mode** — it broadcasts its own open WiFi network so you can configure it from any phone or laptop.

---

## Step 1 — Power On

Connect the ESP32 to USB or a 5V supply. Within a few seconds it broadcasts:

```
SSID:     SQM-Setup
Password: (none — open network)
```

---

## Step 2 — Connect to the Hotspot

Connect your phone or laptop to **SQM-Setup**. No password required.

=== "iOS / Android"
    A **"Sign in to network"** notification will appear automatically. Tap it to open the captive portal in your browser.

=== "macOS"
    A sign-in sheet pops up automatically in Safari after joining the network.

=== "Windows"
    Click the network notification or open a browser — Windows will redirect you to the portal.

=== "Linux"
    Open a browser and navigate to `http://192.168.4.1` manually.

---

## Step 3 — Configure WiFi

The captive portal shows a list of nearby networks:

1. Select your home/lab network
2. Enter the password
3. Tap **Connect**

The device restarts and joins your network. The hotspot disappears.

!!! note "2.4 GHz only"
    ESP32 does not support 5 GHz. If your router broadcasts both bands under the same SSID, the device will pick the 2.4 GHz band automatically.

---

## Step 4 — Find the Device

After connecting, SQMeter is reachable at:

```
http://sqmeter.local      # mDNS — works on most networks
http://<device-ip>        # Direct IP — always works
```

The IP address is logged over serial (115200 baud) if you have a monitor connected. You can also check your router's DHCP client list for a host named `sqmeter`.

---

## Step 5 — Access the Dashboard

Open the web UI. You should see live sensor readings on the Dashboard within a few seconds of the sensors initialising.

---

## WiFi Config is Persistent

Credentials live in the NVS partition — completely separate from firmware and filesystem storage:

| Action | WiFi config |
|--------|------------|
| Flash new firmware | ✅ Preserved |
| Update web UI filesystem | ✅ Preserved |
| Power cycle | ✅ Preserved |
| Full chip erase | ❌ Cleared |

To force the hotspot again (reset WiFi config):

```bash
esptool.py --chip esp32 --port PORT erase_flash
```

Then re-flash from the release binaries.
