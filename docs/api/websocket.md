---
description: "SQMeter WebSocket API: live sensor readings from /ws/sensors every second and device status from /ws/status, with no polling."
---

# WebSocket

Connect to `/ws/sensors` for a live stream of sensor readings, and `/ws/status` for the device status. The first message arrives as soon as you connect; after that `/ws/sensors` pushes every second and `/ws/status` every 2 seconds.

!!! note "Data freshness"
    The default sensor poll interval is 5 seconds, so several WebSocket messages can contain the same sensor reading. Each message carries `dataAgeMs` and `dataStale`, and every sensor group its own `ageMs`.

---

## Connecting

```javascript
const ws = new WebSocket('ws://sqmeter.local/ws/sensors');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('SQM:', data.sky.sqm);
};

ws.onclose = () => {
  // reconnect with backoff
};
```

---

## Message Format

Each message on `/ws/sensors` is the readings document - the same as [`GET /api/sensors`](rest.md#get-apisensors) and MQTT `<base>/state` - with the `safety` object added:

```json
{
  "timestamp": 1791401772,
  "timeValid": true,
  "dataAgeMs": 412,
  "dataStale": false,
  "light": { "status": "ok", "ageMs": 400, "lux": 0.0003, "...": "..." },
  "sky": { "status": "ok", "source": "sqm", "sqm": 21.48, "nelm": 6.2, "bortle": 2, "...": "..." },
  "skySensors": { "sqm": { "status": "ok", "sqm": 21.48, "...": "..." }, "sqml": { "status": "ok", "sqm": 21.73, "...": "..." }, "deltaSqm": 0.25 },
  "environment": { "status": "ok", "temperature": 12.3, "humidity": 64.7, "pressure": 1013.4, "dewpoint": 6.1, "...": "..." },
  "infrared": { "status": "ok", "skyTemperature": -24.7, "ambientTemperature": 12.4, "...": "..." },
  "clouds": { "status": "ok", "coverPercent": 3, "condition": "clear", "...": "..." },
  "rain": { "status": "ok", "raining": false, "intensity": 0.0, "...": "..." },
  "battery": { "present": true, "status": "ok", "percent": 82.4, "state": "discharging", "...": "..." },
  "safety": { "safe": true, "reasons": [], "...": "..." }
}
```

See [MQTT → Readings](../user-guide/mqtt.md#readings-basestate) for every field and the rules: units, `status` values (`ok`, `missing`, `error`, `stale`), and groups for disabled hardware being left out.

!!! note "The same in every language"
    Messages never change with the UI language. Numbers are JSON numbers with `.` as the decimal point and no thousands separators (`12.1`, never `"12,1"`); units and field names are never translated. `safety.reasons` is English text for people to read: key on `safety.reasonFlags` and `safety.safe`.

---

## Python Example

```python
import asyncio
import json
import websockets

async def stream():
    async with websockets.connect("ws://sqmeter.local/ws/sensors") as ws:
        async for message in ws:
            data = json.loads(message)
            sky, clouds = data["sky"], data["clouds"]
            if sky["status"] == "ok":
                cover = clouds.get("description", "unknown")
                print(f"SQM: {sky['sqm']:.2f}  Bortle: {sky['bortle']}  Sky: {cover}")

asyncio.run(stream())
```

---

## `/ws/status`

Each message is the same object as [`GET /api/status`](rest.md#get-apistatus) (WiFi, memory, MQTT, Bluetooth, darkness/sun position, the OLED display, the battery), without the web server diagnostics `connections.tcp` and `connections.stalls`. It is pushed every 2 seconds, and also at once when:

- the recent alerts change: `alerts.recentRevision` goes up whenever [`GET /api/alerts/recent`](rest.md#get-apialertsrecent) would answer differently, so a page fetches the list only then;
- the settings are saved (or a safety hold changes **Force unsafe**): `configRevision` goes up, so open pages reload the settings.

During a GitHub update ([`POST /api/updates/apply`](rest.md#post-apiupdatesapply)) it also carries the update's own messages, which have no status fields:

```json
{"type": "ota_progress", "progress": 42}
{"error": "Not enough space for firmware update"}
```

`progress` is a percentage over both files (the web UI first, then the firmware); an `error` message means the update stopped before the new firmware was switched on, so try it again.

---

## Notes

- The server broadcasts to all connected clients simultaneously, every second for `/ws/sensors` whatever `sensor.readIntervalMs` is
- There is no authentication on the WebSocket endpoints
- Each endpoint takes up to 3 clients. A fourth closes the oldest (counted in `/api/status` → `connections.liveUpdates.replaced`), so reconnect with a backoff rather than opening a new socket on every page
- The device pings a quiet client every 15 seconds and drops one that doesn't answer. A client that stops reading (a tab in a sleeping browser) is skipped while its queue is full and closed after about 10 seconds (`connections.liveUpdates.stalledClosed`)
