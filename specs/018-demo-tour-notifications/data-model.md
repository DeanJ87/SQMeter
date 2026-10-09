# Data Model: Demo Tour and Real Notifications

## TourStep

| Field | Type | Notes |
|---|---|---|
| id | string | stable, used in tests |
| title, body | string / `() => string` | plain words; body may adapt to device state |
| target | CSS selector? | control to outline; none = centred card |
| route | string? | page the step needs (navigated on entry) |
| done | `() => boolean`? | action steps: completes on the device's reaction (FR-002) |
| doIt | `() => void`? | "Do it for me" |

## RealSendSession (memory only)

| Field | Type | Notes |
|---|---|---|
| enabled | boolean | off by default; on only after the confirmation (FR-005) |
| ntfy | `{ topic, token? }` or null | server fixed to `https://ntfy.sh` (R1) |
| pushover | `{ userKey, appToken }` or null | |
| mqtt | `{ url (wss://), topic, username?, password? }` or null | |
| limits | per channel `{ lastAt, count }` | rate limit (R7) |
| results | record id → channel → `{ status, detail }` | shown in the alert list (FR-008) |
| seenRecordId | number | highest record already considered |

Validation: ntfy topic 1-64 of `[A-Za-z0-9_-]`; Pushover keys 30 letters/digits; MQTT URL must be `wss://`.
