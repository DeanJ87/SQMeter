# Contract: Device core (WebAssembly) ↔ demo

`EmulatedDevice` (embind), all documents JSON strings in the device's own shapes:

| Call | Returns | Device equivalent |
|---|---|---|
| `constructor(identityJson)` | - | boot |
| `getConfig(redacted: bool)` | config JSON | `GET /api/config` |
| `applyConfig(json)` | `{success, restartRequired[]}` or `{error}` | `POST /api/config` (same validation) |
| `restart()` | - | `POST /api/restart` (applies queued changes, new boot in history) |
| `tick(nowMs, inputsJson)` | - | one sensor cycle |
| `readings()` | readings document | `GET /api/sensors`, `/ws/sensors`, MQTT `state` |
| `status(systemInfoJson)` | status document | `GET /api/status`, `/ws/status` |
| `safety()` | safety document | `GET /api/safety` |
| `alerts()` / `clearAlerts()` | recent alerts | `GET/DELETE /api/alerts/recent` |
| `setArmed(bool)` / `armed()` | armed state | `/api/alerts/arm`, `/disarm`, `/armed` |
| `testAlert(eventKey, settingsJson)` | the alert that would be sent, per channel "demo" | `POST /api/alerts/test` |
| `alpaca(method, path, paramsJson)` | `{status, contentType, body}` | `/management/*`, `/api/v1/*` |
| `history()` | safety history | `GET /api/safety/history` |
| `mqttMessages()` | topic → payload map it would publish | MQTT publish (shown, never sent) |
| `discovery()` | Home Assistant discovery messages | MQTT discovery (shown, never sent) |

Rules: no call performs I/O; time comes only from `tick`; every document passes the schemas in
`contracts/schemas/`.
