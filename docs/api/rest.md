# REST API

All endpoints are on port 80. Base URL: `http://<device-ip>/api`

---

## Endpoints

### `GET /api/status`

System status, firmware metadata, memory, flash, filesystem, partition, time, WiFi, sensor, GPS, and MQTT diagnostics.

```bash
curl http://sqm-esp32.local/api/status
```

```json
{
  "uptime": 3600,
  "freeHeap": 210432,
  "heapSize": 327680,
  "cpuFreqMHz": 240,
  "flashSize": 4194304,
  "sketchSize": 1048576,
  "freeSketchSpace": 786432,
  "fsTotal": 196608,
  "fsUsed": 40960,
  "firmware": {
    "name": "SQMeter",
    "version": "0.0.1",
    "buildDate": "Apr 25 2026",
    "buildTime": "12:00:00"
  },
  "partitions": {
    "runningSlot": "app0",
    "runningAddress": 65536,
    "runningSize": 1966080,
    "bootSlot": "app0",
    "nextSlot": "app1",
    "nextSize": 1966080,
    "fsAddress": 3997696,
    "fsSize": 196608,
    "nvs": {
      "usedEntries": 24,
      "freeEntries": 96,
      "totalEntries": 120,
      "namespaceCount": 2
    }
  },
  "time": {
    "iso": "2026-04-25T22:14:00+0000",
    "timezone": "UTC0"
  },
  "ntp": {
    "enabled": true,
    "synced": true,
    "status": 2,
    "lastSync": 120000,
    "nextSync": 720000,
    "drift": 0,
    "server": "pool.ntp.org",
    "activeSource": 1,
    "gpsEnabled": false,
    "gpsHasFix": false,
    "gpsTimeUTC": "",
    "gpsSatellites": 0
  },
  "wifi": {
    "connected": true,
    "ssid": "MyNetwork",
    "ip": "192.168.1.42",
    "rssi": -62,
    "mac": "AA:BB:CC:DD:EE:FF"
  },
  "sensors": {
    "tsl2591": { "initialized": true, "status": 0, "lastUpdate": 3595000 },
    "bme280": { "initialized": true, "status": 0, "lastUpdate": 3595000 },
    "mlx90614": { "initialized": true, "status": 0, "lastUpdate": 3595000 },
    "gps": { "initialized": false, "status": 1, "lastUpdate": 0 },
    "rg15": {
      "enabled": true,
      "initialized": true,
      "online": true,
      "stale": false,
      "state": "online",
      "status": 0,
      "lastUpdate": 3595000,
      "uart": {
        "last_command": "R",
        "last_raw_response": "Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h",
        "timeouts": 0,
        "parse_errors": 0,
        "successful_reads": 42
      }
    }
  },
  "mqtt": {
    "enabled": false,
    "connected": false,
    "state": -1,
    "lastPublish": 0,
    "lastReconnectAttempt": 0,
    "broker": "",
    "port": 1883,
    "topic": "sqm/data"
  }
}
```

!!! warning "Known diagnostic gaps"
    The firmware now exposes RG-15 UART diagnostics and sensor freshness information, but it still does not expose every possible system metric such as reset reason, boot count, or `minFreeHeap`. Use serial logs for deeper system bring-up diagnostics.

---

### `GET /api/sensors`

Current sensor readings (point-in-time snapshot).

```bash
curl http://sqm-esp32.local/api/sensors
```

```json
{
  "lightSensor": {
    "lux": 0.0234,
    "rawLux": 0.0231,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "gainName": "MAX",
    "gainFactor": 9876,
    "integrationMs": 600,
    "averagingWindowSeconds": 90,
    "calibrated": true,
    "saturated": false,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.5,
    "rawSqm": 21.42,
    "calibratedSqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site",
    "nightMode": true
  },
  "lightDiagnostics": {
    "rollingVisible": 123.4,
    "correctedVisible": 121.9,
    "darkVisibleOffset": 1.5,
    "sampleCount": 138,
    "rejectedSamples": 0
  },
  "environment": {
    "temperature": 12.4,
    "humidity": 72.1,
    "pressure": 1013.25,
    "dewpoint": 7.8,
    "status": 0
  },
  "irTemperature": {
    "objectTemp": -15.2,
    "ambientTemp": 12.4,
    "status": 0
  },
  "cloudConditions": {
    "temperatureDelta": -27.6,
    "correctedDelta": -24.1,
    "cloudCoverPercent": 5.0,
    "condition": 0,
    "description": "Clear",
    "humidityUsed": 72.1
  },
  "gps": {
    "hasFix": true,
    "satellites": 8,
    "latitude": 51.5074,
    "longitude": -0.1278,
    "altitude": 42.0,
    "hdop": 1.2,
    "age": 800
  },
  "rainSensor": {
    "enabled": true,
    "sensor": "hydreon_rg15",
    "initialized": true,
    "online": true,
    "stale": false,
    "state": "online",
    "timestamp": 1234567890,
    "ageMs": 40,
    "status": 0,
    "isRaining": false,
    "raining": false,
    "acc": 0.000,
    "eventAcc": 2.400,
    "event_accumulation": 0.000,
    "hydreon_event_accumulation": 2.400,
    "totalAcc": 12.340,
    "rInt": 0.000,
    "lensBad": false,
    "emSat": false,
    "uart": {
      "configured": true,
      "opened": true,
      "rx_pin": 18,
      "tx_pin": 19,
      "baud_rate": 9600,
      "uart_port": 1,
      "mode": "polling",
      "resolution": "high",
      "units": "metric",
      "debug_uart": false,
      "last_command": "R",
      "last_raw_response": "Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h",
      "last_error": null,
      "timeouts": 0,
      "parse_errors": 0,
      "successful_reads": 42
    }
  }
}
```

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

!!! note "Optional fields"
    The `gps` object is only present if a GPS module is connected and initialised. The `rainSensor` object is present whenever the RG-15 path is compiled into the firmware; check `enabled`, `initialized`, and `online` to distinguish disabled, opened, and live sensor states. All other objects are always present.

### `status` values

| Value | Meaning |
|-------|---------|
| `0` | OK |
| `1` | Sensor not found |
| `2` | Read error |
| `3` | Stale data |

### `POST /api/sensors/rg15/test`

Trigger a manual RG-15 read and return communication diagnostics.

When HTTP auth is enabled, this endpoint requires credentials.

```bash
curl -X POST http://sqm-esp32.local/api/sensors/rg15/test
```

The response includes:

- `command`
- `bytes_written`
- `raw_response`
- `ack`
- `acknowledged`
- `parsed`
- `elapsed_ms`
- `error`
- `hint`

---

### `POST /api/sensors/rg15/reset-total`, `POST /api/sensors/rg15/reboot`

Send the RG-15 its `O` (reset total accumulation) or `K` (reboot) command. Returns `{"ok": true, "command": "O", "message": "..."}`, or 400 if the command failed. Requires HTTP auth when enabled. See [RG-15](../hardware/rg15.md).

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

The device reboots automatically on success.

---

### `POST /api/update/fs`

OTA filesystem update. Send a LittleFS image as `multipart/form-data`.

```bash
curl -X POST http://sqm-esp32.local/api/update/fs \
  -F "filesystem=@sqmeter-littlefs-v0.0.1.bin"
```

Use this endpoint for web UI assets only. It does not update firmware and does not erase NVS configuration.

!!! warning "Unauthenticated update endpoints"
    `/api/update` and `/api/update/fs` are LAN-only convenience endpoints and currently have no HTTP authentication. Keep SQMeter on a trusted network and do not port-forward it.

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

The current SafetyMonitor verdict - the same value served to Alpaca clients as `IsSafe`, also as a numeric `safe` (1/0) - with the reasons behind it. The same object is included as `safety` in every `/ws/sensors` message.

```json
{
  "isSafe": false,
  "safe": 0,
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
| `isSafe` | Reported verdict, after the safe delay |
| `rawSafe` | Instantaneous rule evaluation, before the safe delay |
| `reasons` / `reasonFlags` | Why it's unsafe (bit flags: 0 manual override, 1 no data, 2 stale, 3 sensor fault, 4 cloud, 5 SQM, 6 humidity, 7 dew point, 8 humidity sensor fault, 9 rain, 10 rain sensor fault, 11 wind, 12 gust, 13 wind sensor fault) |
| `secondsUntilSafe` | Remaining safe-delay countdown while `rawSafe` is true but `isSafe` isn't yet |
| `changedAgeMs` | Time since `isSafe` last changed |

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

Alerts on/off, for automations: `{"armed": true, "armWithAlpaca": false}`. `arm`/`disarm` switch immediately (202) and the state survives restarts; while off nothing is sent and paired phones don't ring. POSTs require HTTP auth when enabled. `/api/alerts/recent` also carries `armed`.

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
