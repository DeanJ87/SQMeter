# Configuration

All settings are stored in NVS (Non-Volatile Storage) and survive firmware and filesystem updates. Configure via the web UI (**Settings**) or the REST API.

## Settings pages

**Settings** has six tabs. Changes are kept until you **Save**; a tab with a problem shows a red dot, and settings that only take effect after a restart ask for one when you save.

Some settings only work when something else is on - MQTT alerts need MQTT, rain alerts need the rain sensor, night-only alerts need your location. Such a setting can't be switched on while what it needs is off, and if you switch the other thing off later the setting is kept exactly as it was but marked **Inactive** with the reason and a button that takes you to the fix. The device doesn't act on it until then, and nothing has to be set up again when you switch the other thing back on. Before you save, a note lists anything the change would make inactive. Every such setting is listed in [Settings dependencies](../reference/settings-dependencies.md).

=== "Device"
    Name, password protection, command-line uploads (ArduinoOTA), Bluetooth.

    ![Device settings](../assets/screenshots/settings-device.png)

=== "Network"
    WiFi, mDNS, and MQTT including what's published and Home Assistant discovery - see [MQTT](mqtt.md).

    ![Network settings](../assets/screenshots/settings-network.png)

=== "Time & Location"
    NTP and GPS time, time zone, and the location used for darkness and Sun & Moon.

    ![Time & Location settings](../assets/screenshots/settings-time.png)

=== "Sensors"
    I2C, sky quality (averaging, SQM offset, dark calibration), cloud detection, the RG-15 rain sensor and the anemometer.

    ![Sensor settings](../assets/screenshots/settings-sensors.png)

=== "Safety"
    ASCOM Alpaca and the safety rules - see [ASCOM Alpaca](alpaca.md#safety-rules).

    ![Safety settings](../assets/screenshots/settings-safety.png)

=== "Alerts"
    Events, levels, wording and channels - see [Alerts](alerts.md).

    ![Alert settings](../assets/screenshots/settings-alerts.png)

---

## Full Configuration Reference

```json
{
  "deviceName": "SQM-ESP32",
  "primaryTimeSource": 0,
  "secondaryTimeSource": 1,

  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "hostname": "sqmeter",
    "mdns": true,
    "autoReconnect": true,
    "reconnectDelayMs": 1000,
    "maxReconnectDelayMs": 300000
  },

  "ntp": {
    "enabled": true,
    "server1": "pool.ntp.org",
    "server2": "time.nist.gov",
    "timezone": "UTC0",
    "syncIntervalMs": 600000
  },

  "gps": {
    "enabled": false,
    "rxPin": 17,
    "txPin": 16,
    "baudRate": 9600
  },

  "mqtt": {
    "enabled": false,
    "broker": "mqtt.example.com",
    "port": 1883,
    "username": "",
    "password": "",
    "topic": "sqmeter",
    "publishIntervalMs": 60000,
    "publish": { "sky": true, "environment": true, "clouds": true, "gps": true, "rain": true, "wind": true, "safety": true, "diagnostics": false },
    "homeAssistant": { "enabled": false, "discoveryPrefix": "homeassistant" }
  },

  "ota": {
    "enabled": false,
    "password": ""
  },

  "auth": {
    "enabled": false,
    "username": "admin",
    "password": ""
  },

  "rain": {
    "enabled": false,
    "rxPin": 18,
    "txPin": 19,
    "baudRate": 9600,
    "debugUart": false,
    "mode": "polling",
    "resolution": "high",
    "units": "metric",
    "pollIntervalMs": 5000,
    "rainClearDelayMs": 900000,
    "dailyResetEnabled": false,
    "dailyResetHour": 0,
    "dailyResetMinute": 0
  },

  "sensor": {
    "readIntervalMs": 5000,
    "i2cSDA": 21,
    "i2cSCL": 22,
    "i2cFrequency": 100000
  },

  "skyAveraging": {
    "windowSeconds": 90
  },

  "skyCalibration": {
    "enabled": false,
    "sqmOffset": 0,
    "darkVisibleOffset": 0,
    "darkFullOffset": 0,
    "darkIrOffset": 0,
    "darkSampleCount": 0,
    "darkCalibratedAt": 0
  },

  "cloudDetection": {
    "clearSkyThreshold": -13,
    "cloudyThreshold": -3,
    "humidityCorrection": 0.75
  },

  "location": {
    "set": false,
    "latitude": 0,
    "longitude": 0,
    "showSunMoon": true
  },

  "alpaca": {
    "enabled": false,
    "manualOverrideUnsafe": false,
    "staleAfterSeconds": 30,
    "safeDelaySeconds": 0,
    "cloudCoverEnabled": true,
    "cloudCoverUnsafePercent": 90,
    "sqmMinEnabled": false,
    "sqmMinSafe": 0,
    "humidityMaxEnabled": false,
    "humidityMaxSafe": 100,
    "dewpointMarginEnabled": false,
    "dewpointMarginMinC": 0,
    "rainUnsafeEnabled": true,
    "rainSensorRequired": true,
    "windSpeedUnsafeEnabled": false,
    "windSpeedUnsafeMs": 10,
    "windGustUnsafeEnabled": false,
    "windGustUnsafeMs": 15
  },

  "alerts": {
    "enabled": false,
    "events": {
      "unsafe": { "level": 3, "sound": "", "title": "", "message": "" },
      "safe": { "level": 2, "sound": "", "title": "", "message": "" },
      "rain_started": { "level": 4, "sound": "", "title": "", "message": "" },
      "rain_stopped": { "level": 2, "sound": "", "title": "", "message": "" },
      "sensor_fault": { "level": 4, "sound": "", "title": "", "message": "" },
      "sensor_recovered": { "level": 1, "sound": "", "title": "", "message": "" },
      "dew_risk": { "level": 0, "sound": "", "title": "", "message": "" },
      "clear_sky": { "level": 0, "sound": "", "title": "", "message": "" },
      "clouded_over": { "level": 0, "sound": "", "title": "", "message": "" }
    },
    "dewRiskMarginC": 2,
    "clearSkyCloudPercent": 20,
    "cloudedOverCloudPercent": 70,
    "skyNightOnly": true,
    "safetyNightOnly": true,
    "nightSunAltitudeDeg": -12,
    "sendMode": "any",
    "armWithAlpaca": false,
    "clientSilentSafetySeconds": 120,
    "clientSilentWeatherSeconds": 600,
    "cooldownSeconds": 300,
    "pushover": { "enabled": false, "userKey": "", "appToken": "", "sound": "" },
    "ntfy": { "enabled": false, "server": "https://ntfy.sh", "topic": "", "token": "" },
    "webhook": { "enabled": false, "url": "", "authHeader": "", "insecureTls": false },
    "mqtt": { "enabled": false }
  },

  "ble": {
    "enabled": false,
    "passkey": ""
  },

  "wind": {
    "enabled": false,
    "speedPin": 27,
    "directionEnabled": false,
    "directionPin": 35,
    "kmhPerHz": 2.4,
    "directionOffsetDeg": 0,
    "vanePullupOhms": 10000
  }
}
```

In the web UI these live on **Settings** tabs: Device (name, security, Bluetooth), Network (WiFi, MQTT), Time & Location, Sensors (sensors, rain, wind, calibration), Safety (`alpaca`) and Alerts.

---

## Fields

### Device

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | string | `"SQM-ESP32"` | Friendly name shown in the web UI |
| `primaryTimeSource` | int | `0` | Primary time source: `0` = NTP, `1` = GPS |
| `secondaryTimeSource` | int | `1` | Fallback time source: `0` = NTP, `1` = GPS |

### WiFi

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ssid` | string | — | Network name (2.4 GHz only) |
| `password` | string | — | Network password |
| `hostname` | string | `"sqmeter"` | Network name: `hostname.local` via mDNS and the DHCP host name. Letters, numbers and hyphens, up to 32 |
| `mdns` | bool | `true` | Advertise `hostname.local` and the web service via mDNS |
| `autoReconnect` | bool | `true` | Reconnect on WiFi drop |
| `reconnectDelayMs` | int | `1000` | Initial reconnect delay (ms) |
| `maxReconnectDelayMs` | int | `300000` | Max reconnect backoff — 5 min |

### NTP

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `true` | Enable NTP time sync |
| `server1` | string | `"pool.ntp.org"` | Primary NTP server |
| `server2` | string | `"time.nist.gov"` | Fallback NTP server |
| `timezone` | string | `"UTC0"` | POSIX timezone string |
| `syncIntervalMs` | int | `600000` | Re-sync interval — default 10 minutes |

!!! tip "POSIX timezone strings"
    Examples: `GMT0`, `EST5EDT,M3.2.0,M11.1.0`, `PST8PDT,M3.2.0,M11.1.0`, `CET-1CEST,M3.5.0,M10.5.0/3`.
    A full list is available at [timezonedb.com](https://timezonedb.com).

### GPS

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable GPS module |
| `rxPin` | int | `17` | ESP32 RX pin (connects to GPS TX) |
| `txPin` | int | `16` | ESP32 TX pin (connects to GPS RX) |
| `baudRate` | int | `9600` | GPS module baud rate (typically 9600) |

When GPS is enabled and has a fix, it can serve as the primary time source for accurate timestamps independent of network connectivity.

### Location

| Field | Default | Description |
|---|---|---|
| `location.set` | `false` | Whether coordinates have been entered. A GPS fix takes precedence |
| `location.latitude` / `location.longitude` | `0` | Decimal degrees, used for darkness (sky alerts) and the Sun & Moon card |
| `location.showSunMoon` | `true` | Show the Sun & Moon card on the dashboard: twilight phase, tonight's astronomical dark, moon phase and illumination, and the next sunrise/sunset and moonrise/moonset. Computed in the browser |

### MQTT

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable MQTT publishing |
| `broker` | string | — | Broker hostname or IP |
| `port` | int | `1883` | Broker port |
| `username` | string | `""` | Auth username (leave empty if none) |
| `password` | string | `""` | Auth password |
| `topic` | string | `"sqmeter"` | Base topic: letters, numbers, `_`, `-`, with `/` between levels. See [MQTT](mqtt.md#topics) |
| `publishIntervalMs` | int | `60000` | How often `<topic>/state` is published (1 s – 24 h) |
| `publish.sky`, `.environment`, `.clouds`, `.gps`, `.rain`, `.wind` | bool | `true` | Groups included in `<topic>/state` |
| `publish.safety` | bool | `true` | Publish `<topic>/safe` and `<topic>/safety` |
| `publish.diagnostics` | bool | `false` | Publish `<topic>/diagnostics` |
| `homeAssistant.enabled` | bool | `false` | Home Assistant MQTT discovery |
| `homeAssistant.discoveryPrefix` | string | `"homeassistant"` | Discovery prefix |

### ArduinoOTA

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable command-line ArduinoOTA uploads |
| `password` | string | `""` | Needed for command-line ArduinoOTA to start |

ArduinoOTA only runs when `ota.enabled` is `true` and `ota.password` is set. Switched on without a password it is saved but inactive ("Set an upload password"). The web UI OTA upload page is separate from command-line ArduinoOTA.

### HTTP Authentication

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Require credentials for mutation endpoints |
| `username` | string | `"admin"` | Username for HTTP Basic Auth |
| `password` | string | `""` | Password - a save with auth on and no password is refused: "HTTP auth password is required when auth is enabled" |

When `auth.enabled` is `true`, the following endpoints require HTTP Basic Auth credentials:

- `POST /api/config` — save configuration
- `POST /api/restart` — restart device
- `POST /api/update` — firmware OTA upload
- `POST /api/update/fs` — filesystem OTA upload
- `POST /api/wifi/connect` — change WiFi network
- `POST /api/mqtt/test` — test MQTT connection

Read-only endpoints remain accessible without credentials:

- `GET /api/sensors`, `GET /api/status`, `GET /api/config`
- WebSocket streams (`/ws/sensors`, `/ws/status`)

The `auth.password` field is masked in `GET /api/config` responses. Send the mask placeholder or an empty string to preserve the stored password; send `null` to clear it.

### Rain Sensor

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Enable Hydreon RG-15 rain sensor |
| `rxPin` | int | `18` | ESP32 RX pin (connects to RG-15 TX) |
| `txPin` | int | `19` | ESP32 TX pin (connects to RG-15 RX) |
| `baudRate` | int | `9600` | RG-15 serial baud rate |
| `debugUart` | bool | `false` | Log RG-15 UART command/response traffic |
| `mode` | string | `"polling"` | Compatibility field; RG-15 communication is polling-only |
| `resolution` | string | `"high"` | `"high"`, `"low"`, or `"switch"` |
| `units` | string | `"metric"` | `"metric"`, `"imperial"`, or `"switch"` |
| `pollIntervalMs` | int | `5000` | Interval between RG-15 `R` commands in polling mode |
| `rainClearDelayMs` | int | `900000` | Local rain latch clear delay after intensity returns to zero |
| `dailyResetEnabled` | bool | `false` | Reset RG-15 total accumulation once per day |
| `dailyResetHour` | int | `0` | Local hour for scheduled total reset |
| `dailyResetMinute` | int | `0` | Local minute for scheduled total reset |

### Sensor

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `readIntervalMs` | int | `5000` | How often sensors are polled (ms) |
| `i2cSDA` | int | `21` | SDA GPIO pin |
| `i2cSCL` | int | `22` | SCL GPIO pin |
| `i2cFrequency` | int | `100000` | I2C clock speed (Hz) |

The TSL2591 light sensor is sampled separately at approximately 600 ms cadence so dark-sky rolling averages are not limited by `readIntervalMs`.

### Sky Averaging

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `windowSeconds` | int | `90` | Rolling raw-count averaging window for night SQM readings. Valid range is 10-300 seconds. |

### Sky Calibration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Apply `sqmOffset` to the rolling SQM result |
| `sqmOffset` | float | `0` | Absolute SQM correction after comparison with a reference SQM/SQM-L |
| `darkVisibleOffset` | float | `0` | Saved dark visible-count floor subtracted before lux/SQM conversion |
| `darkFullOffset` | float | `0` | Reserved dark full-channel statistic |
| `darkIrOffset` | float | `0` | Reserved dark IR-channel statistic |
| `darkSampleCount` | int | `0` | Number of rolling samples used when dark calibration was stored |
| `darkCalibratedAt` | int | `0` | Epoch timestamp when available, otherwise device milliseconds |

To set the dark floor, cover the lens/baffle aperture with an opaque cap, wait for `skyAveraging.windowSeconds`, then call `POST /api/sensors/tsl2591/calibrate-dark`.

### Cloud Detection

Cloud cover comes from the MLX90614: how much colder the sky is than the air (sky minus ambient, °C), corrected for humidity.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `clearSkyThreshold` | float | `-13` | Corrected delta below this is clear (0%) |
| `cloudyThreshold` | float | `-3` | Corrected delta at or above this is overcast (100%); linear in between |
| `humidityCorrection` | float | `0.75` | How strongly humidity is corrected for |

### Safety rules (`alpaca`)

The SafetyMonitor verdict served to N.I.N.A., shown on the dashboard and used for alerts. See [ASCOM Alpaca](alpaca.md#safety-rules) for how the rules combine.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Serve the Alpaca SafetyMonitor and ObservingConditions devices |
| `manualOverrideUnsafe` | bool | `false` | Force unsafe |
| `staleAfterSeconds` | int | `30` | Sensor data older than this is unsafe (1-3600) |
| `safeDelaySeconds` | int | `0` | Conditions must stay safe this long before reporting safe; also runs from boot (0-3600) |
| `cloudCoverEnabled` / `cloudCoverUnsafePercent` | bool / float | `true` / `90` | Unsafe at or above this cloud cover (0-100) |
| `sqmMinEnabled` / `sqmMinSafe` | bool / float | `false` / `0` | Unsafe below this SQM (0-30) |
| `humidityMaxEnabled` / `humidityMaxSafe` | bool / float | `false` / `100` | Unsafe above this humidity (0-100) |
| `dewpointMarginEnabled` / `dewpointMarginMinC` | bool / float | `false` / `0` | Unsafe when temperature minus dew point is below this (0-20 °C) |
| `rainUnsafeEnabled` | bool | `true` | Unsafe while the RG-15 reports rain, and until `rain.rainClearDelayMs` after it stops |
| `rainSensorRequired` | bool | `true` | Unsafe if the enabled RG-15 is offline, stale or reports a lens fault |
| `windSpeedUnsafeEnabled` / `windSpeedUnsafeMs` | bool / float | `false` / `10` | Unsafe at or above this wind speed (m/s, up to 60) |
| `windGustUnsafeEnabled` / `windGustUnsafeMs` | bool / float | `false` / `15` | Unsafe at or above this gust (m/s, up to 80) |

### Alerts

See [Alerts](alerts.md) for events, levels and channels.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Send alerts at all |
| `events.<event>.level` | int | see above | 0 off, 1 quiet, 2 normal, 3 urgent, 4 wake me |
| `events.<event>.sound` | string | `""` | Pushover sound; empty uses `pushover.sound` |
| `events.<event>.title` / `.message` | string | `""` | Your own wording with `{variables}` (up to 80 / 240 characters); empty uses the default |
| `dewRiskMarginC` | float | `2` | Dew risk when temperature is within this of the dew point |
| `clearSkyCloudPercent` / `cloudedOverCloudPercent` | float | `20` / `70` | "Skies clear up" below / "cloud over" above |
| `skyNightOnly` / `safetyNightOnly` | bool | `true` / `true` | Sky / safe-unsafe alerts only while it's dark |
| `nightSunAltitudeDeg` | float | `-12` | "Dark" means the sun below this (-0.833 sunset, -12 nautical, -18 astronomical) |
| `sendMode` | string | `"any"` | When to send: `any` (any time, unless paused) or `whileConnected` (only while an imaging app has an Alpaca device connected) |
| `armWithAlpaca` | bool | `false` | The older form of `sendMode` (`true` = `whileConnected`): still written for older firmware, and read when `sendMode` is missing |
| `clientSilentSafetySeconds` / `clientSilentWeatherSeconds` | int | `120` / `600` | "The imaging app stops checking" after this long without a request to the safety monitor / weather device (30-3600) |
| `cooldownSeconds` | int | `300` | Minimum gap between alerts of the same kind (0-86400) |
| `pushover`, `ntfy`, `webhook`, `mqtt` | object | off | Channel settings; secrets are masked in `GET /api/config` |

Events: `unsafe`, `safe`, `rain_started`, `rain_stopped`, `sensor_fault`, `sensor_recovered`, `dew_risk`, `clear_sky`, `clouded_over`, `client_lost`, `client_back`, `client_disconnected`. The imaging-app events and the silence times are stored under their own NVS key, so they don't count towards the 3900-byte limit on the rest of the alert settings.

Paused or sending is live state, not a setting - see [When to send](alerts.md#when-to-send).

### Bluetooth (`ble`)

Only used by the Bluetooth firmware build. See [Bluetooth](ble.md).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Turn on Bluetooth (slows WiFi - one shared radio) |
| `passkey` | string | `""` | 6-digit pairing passkey for the phone alarm; empty turns the alarm off. Masked in `GET /api/config` |

### Wind

See [Wind (anemometer)](../hardware/wind.md).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enabled` | bool | `false` | Anemometer fitted |
| `speedPin` | int | `27` | Anemometer pulse input |
| `directionEnabled` | bool | `false` | Wind vane fitted |
| `directionPin` | int | `35` | Vane input - must be an ADC1 pin |
| `kmhPerHz` | float | `2.4` | Anemometer calibration (2.4 km/h per Hz for the common Misol/Davis-style cups) |
| `directionOffsetDeg` | float | `0` | Rotate the vane reading to true north |
| `vanePullupOhms` | float | `10000` | Pull-up resistor in the vane divider |

---

## Security Notes

SQMeter is designed for a trusted LAN environment. See the [Security Guide](security.md) for a full threat model and setup recommendations.

- By default there is no HTTP authentication; read-only and mutation endpoints are both accessible to any LAN device.
- Enable `auth.enabled` with a strong password to protect mutation endpoints (config save, OTA, restart, WiFi change).
- `GET /api/config` redacts stored secrets (WiFi, MQTT, OTA, and auth passwords), but still exposes non-secret settings.
- HTTP traffic is plaintext, so credentials sent via config updates are visible to LAN observers. TLS is not supported on the ESP32 in this firmware.
- Command-line ArduinoOTA is disabled unless `ota.enabled` is `true` and `ota.password` is set.
- `rain.debugUart` only affects serial logging for RG-15 command/response traffic. It does not log WiFi, MQTT, or OTA secrets.

Keep the device on a private network or isolated observatory VLAN. Do not expose port 80 or ArduinoOTA to the public internet.

Some settings are applied immediately, but hardware bus settings such as I2C pins, GPS pins, and RG-15 pins may require a restart because the sensors are initialised during firmware startup.

---

## Updating via API

```bash
# Read current config
curl http://sqmeter.local/api/config

# Partial update — only the fields you include are changed
curl -X POST http://sqmeter.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"deviceName": "backyard-sqm", "ntp": {"server1": "time.google.com"}}'
```

Password fields returned by `GET /api/config` are masked as `********`. Sending a masked or empty password back to `POST /api/config` preserves the stored value; send `null` for a password field only when you intentionally want to clear it.

---

## Backup & Restore

```bash
# Backup
curl http://sqmeter.local/api/config > config-backup.json

# Restore
curl -X POST http://sqmeter.local/api/config \
  -H "Content-Type: application/json" \
  -d @config-backup.json
```
