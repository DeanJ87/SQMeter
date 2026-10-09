# Contract: settings in effect

All additions are additive; no existing field changes (constitution: compatible interfaces).

## `GET /api/settings/effective` (new)

- **Auth**: none (read-only, no secrets - like `/api/status`).
- **Schema**: [`specs/016-demo-device-emulation/contracts/schemas/settings-effective.schema.json`](../../016-demo-device-emulation/contracts/schemas/settings-effective.schema.json), checked against the demo by `web/src/demo/__tests__/contracts.test.ts` and against a real device by `tools/contract-check.py`.
- **Served by**: the firmware (`src/WebServer.cpp`), the demo (`tools/demo-core/bridge.cpp` → `web/src/demo/handlers.ts`), the dev mocks (`web/src/mocks/handlers.ts`).

```json
{
  "facts": {
    "wifiConnected": true, "mqttConnected": false, "clockSet": true,
    "gpsRunning": false, "gpsFix": false,
    "bluetoothBuild": false, "bluetoothRunning": false, "pairedPhones": 0,
    "lightDetected": true, "infraredDetected": true, "environmentDetected": true
  },
  "settings": [
    { "id": "D-01", "setting": "alerts.mqtt.enabled", "state": "inactive", "reason": "mqtt-off", "text": "MQTT is off", "fix": "network#mqtt" },
    { "id": "D-14", "setting": "mqtt.publish.rain", "state": "inactive", "reason": "mqtt-off", "text": "MQTT is off", "fix": "network#mqtt", "neutral": true },
    { "id": "D-17", "setting": "alpaca.cloudCoverEnabled", "state": "active", "unmet": "fail-safe" }
  ]
}
```

| Field | Rule |
|---|---|
| `settings[]` | Every reported catalogue setting (`kind: setting` in `lib/SettingsDeps/catalogue.json`), in catalogue order. |
| `state` | `off` \| `active` \| `inactive`. The device never reports `unknown`. |
| `reason`, `text`, `fix` | Only when `inactive`; `fix` is `tab#anchor` (Settings tab and section id) or `restart`. |
| `unmet` | Only for safety rules: `inactive` or `fail-safe`. |
| `neutral` | Only when `true`. |

## `GET /api/safety`, `/api/sensors` → `safety`, MQTT `<base>/safety` (changed, additive)

- New field `rulesNotInEffect: string[]` (always present): rules switched on whose unmet behaviour is `inactive` and whose dependency is off, e.g. `"Unsafe while raining - rain sensor is off"`.
- Schemas updated: `safety.schema.json`, `readings.schema.json` (`safety` object).

## Alert delivery records (`GET /api/alerts/recent`) (behaviour)

- A channel switched on but inactive for a reason other than "alerts are off" is recorded `skipped` with the reason text (e.g. `"MQTT is off"`, `"Not connected to WiFi"`); no delivery is attempted. `failed` is only for real delivery attempts.
- `POST /api/alerts/test` on such a channel is accepted (202) and records `skipped` with the reason.

## Home Assistant discovery (behaviour)

- The `switch/<id>/alerts/config` entity is announced only while alerts can go out (alerts on, or paired phones that can ring); otherwise its config topic is cleared (empty retained payload).

## Settings validation (behaviour)

- `ota.enabled` with an empty `ota.password` is accepted by both the device and the web UI (D-32 is a dependency).
- The web UI's constraint messages equal the device's: "At least one time source must be enabled", "Primary time source is disabled", "Secondary time source is disabled", "MQTT broker and topic are required when MQTT is enabled", "HTTP auth password is required when auth is enabled". The UI no longer requires an auth username (the device never did).
