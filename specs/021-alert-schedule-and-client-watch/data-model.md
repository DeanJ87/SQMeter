# Data model: Alert schedule and client watch

## SendMode (persisted, `alerts.sendMode`)

| Value | Meaning |
|---|---|
| `any` | Alerts are sent unless paused. |
| `whileConnected` | Alerts are sent while an Alpaca client has either device connected; a clean disconnect of the last one pauses them. |

`alerts.armWithAlpaca` (legacy) is always written as `sendMode == "whileConnected"`; on input it is
only used when `sendMode` is absent.

## Schedule state (live, persisted in NVS `sqm-alerts`)

| Field | Type | Notes |
|---|---|---|
| `sending` (`armed`) | bool | The REST/MQTT "armed" flag. |
| `reason` | enum | `none`, `user-ui`, `user-rest`, `user-mqtt`, `client-connected`, `client-disconnected`, `waiting-for-client`, `migrated` |
| `sinceMs` | uptime ms | valid only when `sinceKnown` (changed during this boot) |
| `sinceEpoch` | Unix s | 0 = clock unknown |

Transitions (see research D8):

| Event | Mode | Result |
|---|---|---|
| Pause / Resume (UI, REST, MQTT) | any | paused / sending, reason = source |
| Client connects (0 → ≥1 device connected) | whileConnected | sending, `client-connected` |
| Last device cleanly disconnected | whileConnected | paused, `client-disconnected` |
| Mode `any → whileConnected`, nothing connected | – | paused, `waiting-for-client` |
| Mode `any → whileConnected`, connected | – | sending, `client-connected` |
| Mode `whileConnected → any` while paused by `client-disconnected`/`waiting-for-client` | – | sending, `none` |
| Boot, NVS has `armed=false` but no `reason` | – | paused, `migrated` |

## DeviceActivity (Router, per Alpaca device)

| Field | Type | Notes |
|---|---|---|
| `connected` | bool | Alpaca `Connected` |
| `requests` | u32 counter | every device request, any method |
| `disconnects` | u32 counter | clean disconnects of a connected device |
| `clientId` / `hasClientId` | u32 / bool | last `ClientID` parameter |

## ClientState (ClientWatch, per device)

| Field | Notes |
|---|---|
| `connected` | from the Router |
| `watching` | connected, or requested since the restart (and not cleanly disconnected since) |
| `silent` | watching and no request for longer than the device's silence time |
| `lastRequestMs` / `everRequested` | uptime of the newest request |
| `disconnectedNow` | a clean disconnect happened since the last update (edge) |

Reset when Alpaca is disabled.

## Event settings (persisted)

| Key | Default level | Default title / message |
|---|---|---|
| `client_lost` | 3 Urgent | "Imaging app stopped checking" / "No request to the {device} for {silent_for} - last checked {last_checked}." |
| `client_back` | 1 Quiet | "Imaging app is back" / "The {device} is being checked again." |
| `client_disconnected` | 0 Off | "Imaging app disconnected" / "The {device} was disconnected." |

`alerts.clientSilentSafetySeconds` (default 120) and `alerts.clientSilentWeatherSeconds`
(default 600), each 30–3600.
