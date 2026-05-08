# WebSocket

Connect to `/ws/sensors` for a live stream of sensor readings. The device currently pushes messages every second.

!!! note "Data freshness"
    The default sensor poll interval is 5 seconds, so several WebSocket messages can contain the same sensor reading. The payload now includes `dataTimestamp`, `dataAgeMs`, and `dataStale`.

---

## Connecting

```javascript
const ws = new WebSocket('ws://sqm-esp32.local/ws/sensors');

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
  "lightSensor": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45,
    "full": 168,
    "status": 0
  },
  "skyQuality": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site"
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
    "acc": 0.000,
    "eventAcc": 0.000,
    "totalAcc": 12.340,
    "rInt": 0.000,
    "lensBad": false,
    "emSat": false,
    "uart": {
      "last_command": "R",
      "last_raw_response": "Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h",
      "timeouts": 0,
      "parse_errors": 0,
      "successful_reads": 42
    }
  }
}
```

!!! note "GPS field"
    `gps` is only included in the message when a GPS module is connected and initialised.

!!! note "Rain sensor field"
    `rainSensor` is included whenever the RG-15 path is compiled into the firmware. Check `enabled`, `initialized`, and `online` to distinguish disabled, UART-opened, and live sensor states. Values use the RG-15 configured units; the payload also includes UART diagnostics for bring-up debugging.

### `status` values

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
    async with websockets.connect("ws://sqm-esp32.local/ws/sensors") as ws:
        async for message in ws:
            data = json.loads(message)
            sqm = data["skyQuality"]["sqm"]
            bortle = data["skyQuality"]["bortle"]
            cloud = data["cloudConditions"]["description"]
            print(f"SQM: {sqm:.2f}  Bortle: {bortle}  Sky: {cloud}")

asyncio.run(stream())
```

---

## Notes

- The server broadcasts to all connected clients simultaneously
- There is no authentication on the WebSocket endpoint
- Broadcasts happen every 1 second regardless of `sensor.readIntervalMs`
