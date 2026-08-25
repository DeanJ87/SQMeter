# ASCOM Alpaca

SQMeter can act as an ASCOM Alpaca **SafetyMonitor** and **ObservingConditions** device directly - no separate bridge/service needed. N.I.N.A. and other ASCOM Alpaca clients connect straight to the device's IP address.

!!! note "Replaces the standalone bridge"
    Earlier setups used a separate `SQMeter-ASCOM-Alpaca` Windows service/`.exe` that polled the device's REST API and re-served it as Alpaca. That bridge still works, but is no longer necessary - the device now speaks Alpaca natively. If you're migrating from it, disconnect N.I.N.A. from the bridge's devices first, then follow this guide to connect directly to the device instead.

---

## Enabling Alpaca support

1. Open the web UI and go to **Settings**
2. Scroll to **ASCOM Alpaca** and check **Enable Alpaca SafetyMonitor / ObservingConditions**
3. Set your safety thresholds (see [Safety rules](#safety-rules) below) and **Save**
4. Restart the device (Settings save doesn't require it, but the UDP discovery listener that N.I.N.A. uses to auto-find the device only starts at boot)

Alpaca support is disabled by default. With it off, every Alpaca endpoint still responds (so tooling doesn't 404) but reports `connected: false` and a `NotConnected` error - it just isn't discoverable or usable until enabled.

---

## Connecting from N.I.N.A.

### SafetyMonitor

1. Equipment → **Safety Monitor** → select **ASCOM Alpaca**
2. Click **Refresh** - N.I.N.A. broadcasts a UDP discovery request on port `32227`; SQMeter responds and N.I.N.A. lists **SQMeter SafetyMonitor**
3. Select it and click **Connect**

### ObservingConditions

1. Equipment → **Weather** (Observing Conditions) → select **ASCOM Alpaca**
2. Click **Refresh**, select **SQMeter ObservingConditions**, **Connect**

Both devices are served from the same device/port - connecting one doesn't require or block the other.

### If discovery doesn't find the device

- Confirm **Enable Alpaca...** is checked in Settings and the device has been restarted since
- Discovery is a UDP broadcast - it won't cross VLANs/subnets or most VPNs; N.I.N.A. and the device need to be on the same local network segment
- As a fallback, most Alpaca clients (including N.I.N.A.) let you add a device manually by IP:port instead of relying on discovery - use the device's IP and port `80`

---

## Safety rules

`SafetyMonitor.IsSafe` is computed fresh on every request from the current sensor readings against the thresholds configured in **Settings → ASCOM Alpaca**. In order, any of the following makes it unsafe:

1. **Manual override** - the "Force SafetyMonitor unsafe" checkbox is on
2. **No data yet** - the device hasn't completed a sensor read since boot
3. **Stale data** - the last successful read is older than the stale-data threshold (default 30s)
4. **Sensor fault** - the light sensor (TSL2591) or IR temperature sensor (MLX90614) is reporting a non-OK status
5. **Cloud cover** - at or above the configured threshold (default 90%, if enabled)
6. **Sky brightness (SQM)** - below the configured minimum (disabled by default)
7. **Humidity** - above the configured maximum (disabled by default)
8. **Temperature-dewpoint margin** - below the configured minimum (disabled by default)

Each threshold has its own enable/disable toggle - a disabled threshold never contributes to the verdict. This mirrors the rule set from the standalone bridge it replaces, so behavior should feel identical if you're migrating.

---

## ObservingConditions properties

| Alpaca property | Source |
|---|---|
| `cloudcover` | Cloud detection (IR sky temperature vs. ambient, humidity-corrected) |
| `dewpoint` | BME280 |
| `humidity` | BME280 |
| `skybrightness` | TSL2591 lux |
| `skyquality` | Calculated SQM (mag/arcsec²) |
| `skytemperature` | MLX90614 IR object temperature |
| `temperature` | BME280 |
| `averageperiod` | Always `0` (no averaging is performed) |
| `pressure`, `rainrate`, `starfwhm`, `winddirection`, `windgust`, `windspeed` | Not implemented - no sensor for these; returns Alpaca error `0x400` |

If sensor data is stale or hasn't been read yet, implemented properties return a driver error instead of a stale/zeroed value.

---

## Reference

See the [REST API reference](../api/rest.md#ascom-alpaca-api) for the full endpoint list and the [ASCOM Alpaca API spec](https://ascom-standards.org/api/) for the response envelope and standard error codes.
