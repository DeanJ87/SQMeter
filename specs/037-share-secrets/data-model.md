# Data model: Share passwords with a paired device

## SharingConfig (settings, `Config::sharing`)

| Field | JSON | Type | Default | Notes |
|---|---|---|---|---|
| `secrets` | `sharing.secrets` | bool | `false` | Missing in older settings: `false`. Not a secret: shown by `GET /api/config`, kept in backups. |

Validation: none beyond the type (strict JSON types like every switch). Saving it on without a
device password is allowed; D-53 reports it not in effect.

## Device password in effect

`auth.enabled && !auth.password.empty()`: the condition under which `requireAuth` asks for a login.

## Shared secrets (answer)

| JSON path | Source |
|---|---|
| `mqtt.password` | `Config::mqtt.password` |
| `alerts.ntfy.token` | `Config::alerts.ntfyToken` |
| `alerts.pushover.userKey` | `Config::alerts.pushoverUserKey` |
| `alerts.pushover.appToken` | `Config::alerts.pushoverAppToken` |

## Refusal (enum `SharedSecrets::Refusal`)

| Value | When | HTTP | `error` |
|---|---|---|---|
| `None` | sharing on and device password in effect | (login check next) | |
| `SharingOff` | `!sharing.secrets` | 403 | `Password sharing is off in SQMeter` |
| `NoDevicePassword` | sharing on, device password not in effect | 403 | `Set a web password in SQMeter before sharing passwords` |

## LoginLock

| Field | Meaning |
|---|---|
| `failures` | failed logins in a row (0-5) |
| `lockedUntilMs` | while `now < lockedUntilMs` the request answers 429 |

Transitions: `failed(now)` adds one; the fifth starts a 60 s lock and resets the count.
`succeeded()` resets the count. `retryAfterSeconds(now)` is 0 when not locked, else the whole
seconds left (at least 1). Times are `millis()`; differences are unsigned, so wrap-around is safe.
