# Contract: REST responses

- Success: 2xx. Actions return `{"success": true, ...data}` (200 done, 202 accepted/queued).
- Failure: 4xx (bad request 400, auth 401, conflict 409, not found 404) or 5xx (device failure 500,
  sensor didn't answer 502) with `{"error": "message"}`. Never 200 with a failure flag.
- `GET /api/status` → `sensors`: `{ "<sensor>": { "status", "ageMs" } }` for each present sensor
  (`light`, `environment`, `infrared`, plus `gps`, `rain`, `wind` when enabled; `wind` adds
  `vaneStatus`). → `diagnostics`: `{ "light": {...}, "rain": {...} }` camelCase, `rain` only when
  enabled. No duplicated readings (`gpsData` removed).
- Safety object: `safe` (boolean), `rawSafe`, `reasons`, `reasonFlags`, `secondsUntilSafe`,
  `alpacaEnabled`, `evaluatedAgeMs`, `changedAgeMs`. `GET /api/safe` stays plain `1`/`0`.
- `GET /api/status` → `alerts` and `GET /api/alerts/armed` (spec 021): `armed` (boolean: sending or
  paused), `armWithAlpaca`, `mode` (`any` | `whileConnected`), `reason`, `since` (ISO UTC or `null`),
  `sinceAgeMs` (or `null`). → `alpaca`: `{ "enabled", "clients": { "safetymonitor": {...},
  "observingconditions": {...} } }`, each client `connected`, `watching`, `silent`,
  `lastCheckedAgeMs`, `clientId` (`null` where unknown). See
  [specs/021 contracts](../../021-alert-schedule-and-client-watch/contracts/rest.md).
