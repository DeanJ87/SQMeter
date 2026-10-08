# REST API

All endpoints are on port 80. Base URL: `http://<device-ip>/api`

---

## Conventions

- Keys are camelCase. Readings use the same names and units as `/ws/sensors` and MQTT `<base>/state`.
- **Success** is a 2xx status. Actions return `{"success": true, ...}`: 200 when done, 202 when queued.
- **Failure** is a 4xx or 5xx status with `{"error": "message"}`: 400 bad request, 401 unauthorised, 404 not found, 409 conflict, 500 device failure, 502 a sensor or broker didn't answer.
- Endpoints that change something need the password when [protection](../user-guide/security.md) is on.

---

## Endpoints

### `GET /api/status`

Firmware, memory, flash, partitions, time, WiFi, MQTT, Bluetooth, darkness, sensor health and diagnostics.

```bash
curl http://sqm-esp32.local/api/status
```

```json
{
  "uptime": 3600,
  "freeHeap": 128728,
  "firmware": { "name": "SQMeter", "version": "0.3.0", "buildDate": "Oct  8 2026", "buildTime": "12:00:00", "variant": "standard" },
  "wifi": { "connected": true, "ssid": "MyNetwork", "ip": "192.168.1.42", "rssi": -62, "mac": "AA:BB:CC:DD:EE:FF" },
  "sky": { "locationSource": "manual", "nightKnown": true, "latitude": 51.4779, "longitude": -0.0015, "isNight": false, "sunAltitudeDeg": 19.4 },
  "sensors": {
    "light": { "status": "ok", "ageMs": 400 },
    "environment": { "status": "ok", "ageMs": 3100 },
    "infrared": { "status": "ok", "ageMs": 3100 },
    "rain": { "status": "ok", "ageMs": 40 },
    "wind": { "status": "ok", "ageMs": 900, "vaneStatus": "ok" }
  },
  "diagnostics": {
    "light": { "rollingVisible": 3.1, "correctedVisible": 3.1, "darkVisibleOffset": 0, "sampleCount": 150, "rejectedSamples": 0, "consecutiveSaturatedSamples": 0, "consecutiveLowSamples": 0 },
    "rain": { "state": "online", "uartOpened": true, "rxPin": 18, "txPin": 19, "baudRate": 9600, "uartPort": 1, "lastCommand": "R", "lastResponse": "Acc 0.01 mm, ...", "timeouts": 0, "parseErrors": 0, "successfulReads": 1424, "lastPollAgeMs": 40, "lastResponseAgeMs": 40, "lastSuccessfulReadAgeMs": 40 }
  },
  "mqtt": { "enabled": true, "connected": true, "broker": "192.168.1.10", "port": 1883, "topic": "sqmeter", "availabilityTopic": "sqmeter/availability" }
}
```

`sensors` lists the built-in sensors (`light`, `environment`, `infrared`) and, when enabled, `gps`, `rain` and `wind`. `status` is `ok`, `missing`, `error` or `stale`. `diagnostics.rain` is only present while the rain sensor is enabled. Readings are in `/api/sensors`.

---

### `GET /api/sensors`

The current readings plus the safety verdict. The document is the same as `/ws/sensors` and MQTT `<base>/state`; see [MQTT → Readings](../user-guide/mqtt.md#readings-basestate) for every field.

```bash
curl http://sqm-esp32.local/api/sensors
```

```json
{
  "timestamp": 1791401772,
  "timeValid": true,
  "dataAgeMs": 412,
  "dataStale": false,
  "light": { "status": "ok", "ageMs": 400, "lux": 0.0003, "visible": 307, "infrared": 47, "full": 357, "gain": "MAX", "gainFactor": 9876, "integrationMs": 600, "saturated": false, "nightMode": true },
  "sky": { "status": "ok", "sqm": 21.48, "rawSqm": 21.41, "nelm": 6.2, "bortle": 2, "description": "Typical truly dark site", "calibrated": false, "averagingWindowSeconds": 90 },
  "environment": { "status": "error", "ageMs": 61000 },
  "infrared": { "status": "ok", "ageMs": 3100, "skyTemperature": -24.7, "ambientTemperature": 12.4 },
  "clouds": { "status": "ok", "coverPercent": 3, "condition": "clear", "description": "Clear", "temperatureDelta": -37.1, "correctedDelta": -33.0, "humidity": 53, "humiditySource": "assumed" },
  "safety": { "safe": true, "rawSafe": true, "reasons": [], "reasonFlags": 0, "secondsUntilSafe": 0, "alpacaEnabled": true, "evaluatedAgeMs": 412, "changedAgeMs": 3600000 }
}
```

A group whose sensor isn't `ok` carries only `status` and `ageMs` (like `environment` above).

---

### `POST /api/sensors/tsl2591/calibrate-dark`

Stores the current rolling TSL2591 visible count as the dark visible offset. Cover the aperture with an opaque cap, wait for the rolling window to fill, then call this endpoint.

```bash
curl -X POST http://sqm-esp32.local/api/sensors/tsl2591/calibrate-dark
```

```json
{
  "success": true,
  "darkVisibleOffset": 1.5,
  "sampleCount": 138,
  "darkCalibratedAt": 1778171234
}
```

### `POST /api/sensors/rg15/test`

Trigger a manual RG-15 read and return communication diagnostics.

When HTTP auth is enabled, this endpoint requires credentials.

```bash
curl -X POST http://sqm-esp32.local/api/sensors/rg15/test
```

Success is 200 `{"success": true, "command": "R", "bytesWritten": 2, "elapsedMs": 140, "rawResponse": "Acc 0.00 mm, ...", "ack": "R", "online": true, "lastSuccessfulReadAgeMs": 40}`. With no valid reply it's 502 with `error` and a wiring `hint`, plus the same diagnostic fields.

---

### `POST /api/sensors/rg15/reset-total`, `POST /api/sensors/rg15/reboot`

Send the RG-15 its `O` (reset total accumulation) or `K` (reboot) command. Returns `{"success": true, "command": "O", "message": "..."}`, or 502 with `{"error": "..."}` if the sensor didn't take it. Requires HTTP auth when enabled. See [RG-15](../hardware/rg15.md).

### `GET /api/config`

Read the full device configuration. Password fields are returned as `********` when a stored password exists.

```bash
curl http://sqm-esp32.local/api/config
```

!!! warning "LAN-sensitive endpoint"
    Password fields are redacted in this response, but the endpoint is still unauthenticated and returns operational configuration. Treat `/api/config` as LAN-sensitive and do not expose the device to guest networks or the public internet.

---

### `POST /api/config`

Update configuration. Partial updates are supported — only the fields you send are changed.

```bash
curl -X POST http://sqm-esp32.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"deviceName": "backyard-sqm", "sensor": {"readIntervalMs": 10000}}'
```

Masked or empty password fields preserve the existing stored password. Send `null` for `wifi.password`, `mqtt.password`, or `ota.password` only when you intentionally want to clear that password.

Successful saves return:

```json
{ "success": true }
```

!!! note
    `POST` is the primary method. The firmware also accepts `PUT` for compatibility with full-resource settings clients.

---

### `GET /api/wifi/scan`

Scan for nearby WiFi networks.

```bash
curl http://sqm-esp32.local/api/wifi/scan
```

```json
{
  "networks": [
    { "ssid": "MyNetwork", "rssi": -55, "encryption": "secured" },
    { "ssid": "Neighbour", "rssi": -80, "encryption": "secured" }
  ]
}
```

!!! warning "Blocking scan"
    This endpoint currently calls `WiFi.scanNetworks()` synchronously inside the async web-server handler. Avoid repeated scans while streaming WebSocket data or performing OTA updates.

---

### `POST /api/wifi/connect`

Connect to a WiFi network and save credentials to NVS.

```bash
curl -X POST http://sqm-esp32.local/api/wifi/connect \
  -H "Content-Type: application/json" \
  -d '{"ssid": "MyNetwork", "password": "hunter2"}'
```

---

### `POST /api/restart`

Restart the device.

```bash
curl -X POST http://sqm-esp32.local/api/restart
```

---

### `POST /api/update`

OTA firmware update. Send a raw `.bin` file as `multipart/form-data`.

```bash
curl -X POST http://sqm-esp32.local/api/update \
  -F "firmware=@sqmeter-firmware-v0.0.1.bin"
```

Returns 200 `{"success": true}` and reboots, or 500 `{"error": "..."}` (for example "Could not activate partition") and keeps the running firmware.

---

### `POST /api/update/fs`

OTA filesystem update. Send a LittleFS image as `multipart/form-data`.

```bash
curl -X POST http://sqm-esp32.local/api/update/fs \
  -F "filesystem=@sqmeter-littlefs-v0.0.1.bin"
```

Use this endpoint for web UI assets only. It does not update firmware and does not erase NVS configuration. Responses are as for `/api/update`.

Both upload endpoints need the password when protection is on. Keep SQMeter on a trusted network and don't port-forward it.

---

### `POST /api/mqtt/test`

Tries to connect to a broker with the given details, without saving them: `{"broker": "192.168.1.10", "port": 1883, "username": "", "password": "", "clientId": "SQM-Test"}`. Returns 200 `{"success": true, "message": "Connection successful"}`, or 502 `{"error": "Connection timeout", "state": -4}`.

---

### `GET /api/updates/check`

Checks GitHub Releases for the given track and returns matched firmware+filesystem asset pairs. See [OTA Updates](../user-guide/ota.md#check-for-updates-recommended) for the full flow.

```bash
curl "http://sqm-esp32.local/api/updates/check?track=stable"
```

```json
[
  {
    "tag": "v0.1.3",
    "name": "SQMeter v0.1.3",
    "prerelease": false,
    "publishedAt": "2026-06-28T17:35:57Z",
    "firmwareAssetUrl": "https://github.com/DeanJ87/SQMeter/releases/download/v0.1.3/sqmeter-firmware-v0.1.3.bin",
    "firmwareAssetSize": 1123472,
    "fsAssetUrl": "https://github.com/DeanJ87/SQMeter/releases/download/v0.1.3/sqmeter-littlefs-v0.1.3.bin",
    "fsAssetSize": 524288
  }
]
```

`track` is `stable` (default) or `beta`, mapped directly from GitHub's `prerelease` flag. A release without both a `sqmeter-firmware-*.bin` and a `sqmeter-littlefs-*.bin` asset is omitted entirely.

---

### `POST /api/updates/apply`

Starts a self-download-and-flash of a release returned by `check`, using its asset URLs directly.

```bash
curl -X POST http://sqm-esp32.local/api/updates/apply \
  -H "Content-Type: application/json" \
  -d '{
    "firmwareAssetUrl": "https://github.com/DeanJ87/SQMeter/releases/download/v0.1.3/sqmeter-firmware-v0.1.3.bin",
    "firmwareAssetSize": 1123472,
    "fsAssetUrl": "https://github.com/DeanJ87/SQMeter/releases/download/v0.1.3/sqmeter-littlefs-v0.1.3.bin",
    "fsAssetSize": 524288
  }'
```

Returns immediately with `{"success":true,"message":"Update started"}` - the download and flash happen on a background task. Progress and errors are pushed over `/ws/status` as `{"type":"ota_progress","progress":N}` messages (or `{"error":"..."}` on failure). The device reboots automatically once both assets are flashed successfully.

---

## Safety

### `GET /api/safe`

Plain text `1` (safe) or `0` (unsafe) - the SafetyMonitor verdict, for scripts and loggers (`curl -s http://sqmeter.local/api/safe`).

### `GET /api/safety`

The current SafetyMonitor verdict - `safe`, the same value served to Alpaca clients as `IsSafe` - with the reasons behind it. The same object is included as `safety` in every `/ws/sensors` message.

```json
{
  "safe": false,
  "rawSafe": false,
  "alpacaEnabled": true,
  "reasonFlags": 512,
  "reasons": ["Rain detected"],
  "secondsUntilSafe": 0,
  "evaluatedAgeMs": 412,
  "changedAgeMs": 1260000
}
```

| Field | Meaning |
|---|---|
| `safe` | Reported verdict, after the safe delay |
| `rawSafe` | Instantaneous rule evaluation, before the safe delay |
| `reasons` / `reasonFlags` | Why it's unsafe (bit flags: 0 manual override, 1 no data, 2 stale, 3 sensor fault, 4 cloud, 5 SQM, 6 humidity, 7 dew point, 8 humidity sensor fault, 9 rain, 10 rain sensor fault, 11 wind, 12 gust, 13 wind sensor fault) |
| `secondsUntilSafe` | Remaining safe-delay countdown while `rawSafe` is true but `safe` isn't yet |
| `changedAgeMs` | Time since `safe` last changed |

---

### `GET /api/safety/history`

The last 32 safety changes, device restarts and safe/unsafe alerts sent, newest first. Kept in RTC memory, so it survives software restarts, crashes and OTA updates, not power cuts.

```json
{"boot":3,"uptime":4000,"entries":[
  {"kind":"alert","boot":3,"uptime":3900,"timestamp":1759500000,"safe":false},
  {"kind":"change","boot":3,"uptime":3899,"timestamp":1759499999,"safe":false,"held":false,"reasonFlags":48},
  {"kind":"boot","boot":3,"uptime":0,"resetReason":3}]}
```

`held` marks unsafe only because of the safe delay. `resetReason` is ESP-IDF's `esp_reset_reason_t` (1 power on, 3 software restart, 4 crash, 5-7 watchdog, 9 brownout). `timestamp` is missing for entries from before the clock was set.

## Alerts

See [Alerts](../user-guide/alerts.md) for setup.

### `POST /api/alerts/test?channel=<mqtt|pushover|ntfy|webhook|all>`

Queues a test notification on the given (saved and enabled) channel(s). Returns `202 {"success":true}`; delivery happens in the background - check `/api/alerts/recent` for the result. `400` if the channel is unknown or not enabled. Requires HTTP auth when enabled.

Add `event=<unsafe|safe|rain_started|rain_stopped|sensor_fault|sensor_recovered|dew_risk|clear_sky|clouded_over>&level=<1-4>&sound=<pushover sound>` to send a sample of that event (title "Test: ...") at that level and sound instead. Level 4 (wake me) also rings paired Bluetooth phones; with no push channel enabled, it only rings the phones. `title` and `message` (up to 80 / 240 characters) try out custom wording with `{variables}`, filled in from current readings.

### `GET /api/alerts/armed`, `POST /api/alerts/arm`, `POST /api/alerts/disarm`

Alerts on/off, for automations: `{"armed": true, "armWithAlpaca": false}`. `arm`/`disarm` return 202 `{"success": true, "armed": true|false}` and switch immediately and the state survives restarts; while off nothing is sent and paired phones don't ring. POSTs require HTTP auth when enabled. `/api/alerts/recent` also carries `armed`.

### `POST /api/alerts/clear`

Empties the recent-alerts list. Requires HTTP auth when enabled.

### `GET /api/alerts/recent`

Whether alerts are on, and the last 20 alerts since boot, newest first:

```json
{"enabled":true,"alerts":[{"id":2,"event":"rain_started","title":"Rain detected","message":"The rain sensor reports rain (2.4 mm/h).","level":"wake","ageSeconds":42,"timestamp":1759500000,
  "channels":{"pushover":{"status":"sent","detail":"HTTP 200"},"mqtt":{"status":"failed","detail":"MQTT not connected"}}}]}
```

`status` is `pending`, `sent`, `failed` or `skipped`.

---

## Bluetooth phone alarm

Bluetooth firmware build only. See [Bluetooth](../user-guide/ble.md).

### `POST /api/ble/ack`

Acknowledge the ringing phone alarm from the web UI, as if a phone had. 409 if no alarm is active. Requires HTTP auth when enabled.

### `POST /api/ble/forget-bonds`

Unpair every phone; they need to pair again (with the passkey) to get alarms. 409 if Bluetooth is off. Requires HTTP auth when enabled.

## ASCOM Alpaca API

SQMeter can emit itself directly as an ASCOM Alpaca **SafetyMonitor** and **ObservingConditions** device - see [ASCOM Alpaca](../user-guide/alpaca.md) for the full setup guide, N.I.N.A. configuration, and safety-rule reference. Summary of the HTTP surface (all under the same port-80 server as the rest of the API, response envelope per the [ASCOM Alpaca API spec](https://ascom-standards.org/api/)):

| Endpoint | Purpose |
|----------|---------|
| `GET /management/apiversions` | Supported Alpaca API versions |
| `GET /management/v1/description` | Server description |
| `GET /management/v1/configureddevices` | Lists the two devices (empty if Alpaca is disabled in settings) |
| `GET /setup`, `GET /setup/v1/<devicetype>/0/setup` | Redirects to **Settings → ASCOM Alpaca** in the web UI |
| `GET/PUT /api/v1/<device>/0/connected` | Common ASCOM device API. `PUT` requires a `Connected=true\|false` form parameter |
| `PUT /api/v1/<device>/0/connect`, `disconnect`; `GET connecting`, `devicestate` | ASCOM Platform 7 members (SafetyMonitor interface v3, ObservingConditions interface v2). `connecting` is always `false` |
| `GET /api/v1/<device>/0/name`, `description`, `driverinfo`, `driverversion`, `interfaceversion`, `supportedactions` | Common ASCOM device API |
| `PUT /api/v1/<device>/0/action`, `commandblind`, `commandbool`, `commandstring` | Not supported - Alpaca error `0x400` (NotImplemented) |
| `GET /api/v1/safetymonitor/0/issafe` | `true`/`false` from the safety-rule evaluation |
| `GET /api/v1/observingconditions/0/<property>` | One route per Alpaca property: `cloudcover`, `dewpoint`, `humidity`, `pressure`, `rainrate` (RG-15, mm/h), `skybrightness`, `skyquality`, `skytemperature`, `temperature`, `windspeed`/`windgust` (anemometer, m/s), `winddirection` (vane), `averageperiod`. `starfwhm`, and sensors that aren't enabled, return Alpaca error `0x400` (NotImplemented) |
| `PUT /api/v1/observingconditions/0/averageperiod`, `refresh` | `AveragePeriod` must be `0`; `refresh` is a no-op |
| `GET /api/v1/observingconditions/0/sensordescription`, `timesincelastupdate` | Require `SensorName` (a property name; empty = any sensor for `timesincelastupdate`) |

`<device>` is `safetymonitor` or `observingconditions`. Parameter names (`ClientTransactionID`, `Connected`, ...) are case-insensitive, per the Alpaca spec. Any other path under `/api/v1/` (unknown device type/number, method, or HTTP verb) returns HTTP `400` with a plain-text message.

A UDP listener on port `32227` answers Alpaca discovery broadcasts (`alpacadiscovery1` → `{"AlpacaPort":80}`) whenever Alpaca is enabled in settings - this requires a device restart to start/stop, unlike the HTTP routes above which reflect the setting live.
