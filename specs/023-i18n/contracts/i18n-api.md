# Contract: Language API (device and demo)

Mutating routes require auth when auth is on. Declared in tools/api/routes.json, documented in docs/api/rest.md; schema `specs/016-demo-device-emulation/contracts/schemas/i18n.schema.json` (checked by tools/contract-check.py).

- `GET /api/i18n` → `200 { "language": "es", "state": "idle|downloading|installed|failed|restoring", "firmwareVersion": "0.2.1", "pack": { "lang": "es", "version": "0.2.1", "size": 22515 } | null, "error"?: "..." }`
- `GET /lang.json` → the stored file (`Content-Encoding: gzip`, served by the static handler from `/lang.json.gz`); `404` if there is none.
- `POST /api/i18n/install` → `202 { "started": true }` (download the configured language again); `409 { "error" }` while a firmware update or another download runs, or when the language is English.
- `POST /api/i18n/upload?lang=<code>&version=<v>` (multipart `file`) → `200` with the `GET /api/i18n` document; `400 { "error" }` (not gzip, over 64 KB, not enough space, unknown language). The browser checks the file's contents and passes its language and version.
- `POST /api/config` with `{"language": "<code>"}`: validates the code. Switching to `en` deletes the file; switching to another language starts its download.
- Device text stays English on the wire: the UI recognises each message against the `device.*` templates in web/src/i18n/en.json (generated from the firmware by tools/i18n/gen_device_catalog.py) and shows the translation. The planned `errorId`/`reasonMessages` fields and the `/api/i18n/pack` route were dropped to stay within the 12 KB flash budget (research D4).

Release assets (per firmware version, `https://github.com/DeanJ87/SQMeter/releases/download/v<version>/`):

- `sqmeter-i18n-<code>.json.gz`: gzip of `{ "lang", "version", "messages" }`, at most 64 KB.
- `sqmeter-i18n-<code>.json.gz.sha256`: `"<sha256 hex> <size>"`, read by the device before it downloads the file.
- `sqmeter-i18n-manifest.json`: `{ "version", "languages": [{ "code", "file", "size", "sha256" }] }`.
