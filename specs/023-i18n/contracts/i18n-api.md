# Contract: Language API (device and demo)

Mutating routes require auth when auth is on.

- `GET /api/i18n` → `200 { "language": "es", "state": "idle|downloading|installed|failed|restoring", "pack": { "lang": "es", "version": "0.2.1", "size": 18234 } | null, "error"?: "...", "errorId"?: "device.i18n.*", "errorParams"?: {}, "firmwareVersion": "0.2.1" }`
- `GET /api/i18n/pack` → the stored file (`application/json`, `Content-Encoding: gzip`); `404` if there is none.
- `POST /api/i18n/install` → `202 { "started": true }` (download for the configured language again); `409 { "error", "errorId" }` while an OTA update or another download runs, or when the language is English.
- `POST /api/i18n/upload` (multipart `file`) → `200 { "success": true, "pack": {...} }`; `400 { "error", "errorId" }` (not gzip, too big, not enough space).
- `POST /api/config` with `{"language": "<code>"}`: validates the code. Switching to `en` deletes the file; switching to another language starts its download.
- Additive fields (FR-008): error responses gain `errorId` and `errorParams`; safety documents gain `reasonMessages: [{ "id", "params" }]` beside `reasons`; alert history entries gain `titleId`/`titleParams` and `messageId`/`messageParams` when their text is a default template.
