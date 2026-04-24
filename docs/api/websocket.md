# WebSocket

Connect to `/ws` for a real-time stream of sensor readings. The device pushes updates every second — no polling required.

---

## Connecting

```javascript
const ws = new WebSocket('ws://sqm-esp32.local/ws');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('SQM:', data.skyQuality.sqm);
};

ws.onclose = () => {
  // reconnect with backoff
};
```

---

## Message Format

```json
{
  "tsl2591": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "timestamp": 12345,
    "status": 0
  },
  "bme280": {
    "temperature": 12.4,
    "humidity": 72.1,
    "pressure": 1013.25,
    "timestamp": 12345,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site"
  }
}
```

### `status` field

| Value | Meaning |
|-------|---------|
| `0` | OK |
| `1` | Sensor not found |
| `2` | Read error |
| `3` | Stale data |

---

## Python Example

```python
import asyncio
import json
import websockets

async def stream():
    async with websockets.connect("ws://sqm-esp32.local/ws") as ws:
        async for message in ws:
            data = json.loads(message)
            sqm = data["skyQuality"]["sqm"]
            bortle = data["skyQuality"]["bortle"]
            print(f"SQM: {sqm:.2f}  Bortle: {bortle}")

asyncio.run(stream())
```

---

## Notes

- The server broadcasts to all connected clients simultaneously
- There is no authentication on the WebSocket endpoint
- The connection is kept alive with the ESP32's async TCP stack — no manual ping/pong needed
