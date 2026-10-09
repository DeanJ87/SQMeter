# REST contract changes (spec 021)

All changes are additive (FR-021).

## `GET /api/alerts/armed`

```json
{
  "armed": false,
  "armWithAlpaca": true,
  "mode": "whileConnected",
  "reason": "client-disconnected",
  "since": "2026-10-08T05:42:10Z",
  "sinceAgeMs": 734000
}
```

- `mode`: `any` | `whileConnected`.
- `reason`: `none` | `user-ui` | `user-rest` | `user-mqtt` | `client-connected` |
  `client-disconnected` | `waiting-for-client` | `migrated`.
- `since`: ISO 8601 UTC, or `null` when the clock wasn't set at the time.
- `sinceAgeMs`: milliseconds since the change on the uptime clock, or `null` (changed before this
  boot, or never).

## `POST /api/alerts/arm`, `POST /api/alerts/disarm`

Unchanged (auth required, 202 `{success, armed}`). Optional query parameter `source=ui` records
that the web UI paused/resumed; otherwise the source is REST.

## `GET /api/status`

Adds:

```json
"alerts": { "...": "same object as /api/alerts/armed" },
"alpaca": {
  "enabled": true,
  "clients": {
    "safetymonitor": { "connected": true, "watching": true, "silent": false, "lastCheckedAgeMs": 2100, "clientId": 4021 },
    "observingconditions": { "connected": false, "watching": false, "silent": false, "lastCheckedAgeMs": null, "clientId": null }
  }
}
```

## `GET/POST /api/config` → `alerts`

Adds `sendMode`, `clientSilentSafetySeconds`, `clientSilentWeatherSeconds` and
`events.client_lost`, `events.client_back`, `events.client_disconnected` (same shape as other
events). `armWithAlpaca` is still present.

Validation errors:

- "Alerts: when to send must be any or whileConnected"
- "Alerts: silence times must be between 30 and 3600 seconds"

## MQTT

Unchanged: `<topic>/alerts/armed` is `1` while sending and `0` while paused;
`<topic>/alerts/armed/set` pauses/resumes (source `mqtt`). Client events go to `<topic>/alerts`
like every other alert, with `event` = `client_lost` | `client_back` | `client_disconnected`.
