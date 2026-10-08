# WebSocket

Connect to `/ws/sensors` for a live stream of sensor readings. The device currently pushes messages every second.

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
  "sky": { "status": "ok", "sqm": 21.48, "nelm": 6.2, "bortle": 2, "...": "..." },
  "environment": { "status": "ok", "temperature": 12.3, "humidity": 64.7, "pressure": 1013.4, "dewpoint": 6.1, "...": "..." },
  "infrared": { "status": "ok", "skyTemperature": -24.7, "ambientTemperature": 12.4, "...": "..." },
  "clouds": { "status": "ok", "coverPercent": 3, "condition": "clear", "...": "..." },
  "rain": { "status": "ok", "raining": false, "intensity": 0.0, "...": "..." },
  "safety": { "safe": true, "reasons": [], "...": "..." }
}
```

See [MQTT → Readings](../user-guide/mqtt.md#readings-basestate) for every field and the rules: units, `status` values (`ok`, `missing`, `error`, `stale`), and groups for disabled hardware being left out.

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

## Notes

- `/ws/status` streams the same object as [`GET /api/status`](rest.md#get-apistatus) every 2 seconds (WiFi, memory, MQTT, Bluetooth, darkness/sun position)
- The server broadcasts to all connected clients simultaneously
- There is no authentication on the WebSocket endpoint
- Broadcasts happen every 1 second regardless of `sensor.readIntervalMs`
