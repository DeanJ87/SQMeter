# Contract: status and routes

## `/api/status` → `display` (FR-013)

Always present (also on devices without a display):

```json
"display": {
  "enabled": true,
  "detected": true,
  "on": true,
  "page": "sky",
  "nightMode": false
}
```

| Field | Type | Meaning |
|---|---|---|
| `enabled` | bool | `display.enabled` in settings |
| `detected` | bool | a display answered at 0x3C (false while disabled: it is not probed) |
| `on` | bool | lit now |
| `page` | string \| null | page shown (`sky` … `network`), null when off |
| `nightMode` | bool | night layout and minimum brightness in use (night, or night unknown) |

Also sent on `/ws/status` (same document). `Deps::Facts` adds `displayDetected` (in
`/api/settings/effective` `facts`). The status schema
(`specs/016-demo-device-emulation/contracts/schemas/status.schema.json`) and the web `Status` type gain
the object; `STATUS_DOC_CAPACITY` stays sufficient (pinned by `test_status_document_fits`).

## `POST /api/display/wake`

Auth: required (`requireAuth`; constitution VI). No body.

| Case | Status | Body |
|---|---|---|
| enabled and detected | 200 | `{"ok": true}` — display on for the timeout (applied by the loop task within one pass) |
| display off in settings | 409 | `{"error": "Display is off in settings"}` |
| not detected | 409 | `{"error": "Display not detected"}` |
| forced off by power saving (spec 032) | 409 | `{"error": "Display is off to save power"}` |

## `POST /api/display/blank`

Auth: required. No body. 200 `{"ok": true}` — display off now, until the next wake (boot, wake
request, button). Same 409 cases for disabled / not detected.

Both routes are registered before any broader `/api/display` handler (ESPAsyncWebServer prefix
matching), listed in `tools/api/routes.json` (auth `required`) and `tools/api/operations.json`
(tag Device), and reflected in `docs/api/openapi.json` (`tools/api/openapi.py --check`) and
`docs/api/rest.md`. The demo device implements both. Error strings are device-built messages and go
through the device catalogue (`tools/i18n/gen_device_catalog.py`).
