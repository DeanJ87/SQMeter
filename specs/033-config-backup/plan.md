# Implementation Plan: Settings backup and restore

**Branch**: `feature/033-config-backup` | **Date**: 2026-10-18 | **Spec**: [spec.md](spec.md) |
**Research**: [research.md](research.md)

## Summary

A **Backup & restore** card on Settings → Device. Backing up downloads one JSON file: the settings exactly
as `GET /api/config` shows them, a header (format, firmware, board, build, device, time) and, with a
passphrase, the nine secrets encrypted with AES-256-GCM under a PBKDF2-HMAC-SHA256 key (20,000 iterations,
random salt). Restoring uploads the file (and passphrase), and the device answers with a preview computed
from the complete candidate config - changes, secret status, kept, ignored and not-applicable settings,
restart and cut-off warnings - and a token; applying with the token saves exactly that candidate as one
settings save. The format, crypto use, preview and restart rules live in a new pure `lib/ConfigBackup`
with native tests; the firmware adds three authenticated routes whose slow steps run in a background task;
the demo runs the same library through its WebAssembly core.

## Technical Context

**Language/Version**: C++17 (firmware, `lib/`, native tests); TypeScript 5 / Preact (web UI); Python 3 (tools).

**Primary Dependencies**: ArduinoJson 6, ESPAsyncWebServer 3.12, mbedTLS (ESP-IDF 5.5) on the device; a
portable SHA-256/AES-GCM implementation in `lib/` for native tests and the demo (research R2).

**Storage**: none new. The staged preview lives in RAM for at most 10 minutes.

**Testing**: Unity native tests (`pio test -e native`), Vitest (+ MSW), Playwright for the mockups (kept outside the repo).

**Target Platform**: ESP32 (standard and Bluetooth builds), browsers, the WebAssembly demo.

**Project Type**: embedded firmware + web UI.

**Performance Goals**: a backup or a preview in a few seconds on the slowest board (key derivation
1-2 s, research R2); the web server keeps answering throughout.

**Constraints**: no blocking handlers (IV); BLE build at 94.0% of its slot on `main` (1,724,848 bytes; 95%
= 1,743,257), so the feature stays within ~18 KB of flash; UI growth well under 10 KB gzipped (SIZE-01);
request bodies ≤ 20,480 bytes; transient heap ≤ ~60 KB worst case (R7).

**Scale/Scope**: 3 routes, 1 card, a few dozen UI strings × 14 languages, 1 library.

## Constitution Check

| Principle | How this plan satisfies it |
|---|---|
| I. Fail-safe verdict | Not touched; a restore goes through the same validation as a save. |
| II. Alpaca | Not touched (ConformU unaffected). |
| III. Testable pure logic | Format, crypto use, preview, restart rules in `lib/ConfigBackup` with native tests; UI in Vitest. |
| IV. Budgets | Slow work in a pinned background task (R3); flash measured on both builds; bodies capped; heap to be measured on the spare device (open item). |
| V. Quiet UI | One card from the shared parts, explanations in "?", one-line Notes, no banners; mockups first. |
| VI. Trusted-LAN security | All three routes call `requireAuth`; the backup never holds a clear-text secret; previews show secret paths only; the passphrase is never stored or logged. |
| VII. Docs | `configuration.md` Backup & Restore rewritten; REST docs, OpenAPI, security notes. |
| VIII. Quality | Coding standard limits respected; `tools/quality/check.py` clean. |

No violations; Complexity Tracking is empty. Re-checked after Phase 1 design: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/033-config-backup/
├── plan.md  research.md  data-model.md  quickstart.md  tasks.md
└── contracts/  backup-file.md  rest-api.md  ui.md
```

### Source Code

```text
lib/ConfigBackup/
├── include/ConfigBackup.h      file format, preview, restart rules (public API)
├── include/BackupCrypto.h      deriveKey / seal / open (implemented per platform)
├── src/BackupFile.cpp          make and read files, base64, secrets block
├── src/BackupPreview.cpp       candidate, not-applicable, diff, warnings, preview JSON
├── src/BackupRules.cpp         restart reasons, version compare, board pins
└── src/PortableCrypto.cpp      SHA-256/HMAC/PBKDF2/AES-256/GCM (not on ESP_PLATFORM)
src/BackupCryptoMbed.cpp        mbedTLS adapter (firmware)
src/WebServerBackup.cpp         routes, raw-body collection, background job, staged restore
src/WebServerApi.cpp            handleSetConfig shares the save-and-apply step
include/WebServer.h             backup members
test/test_config_backup/        native tests
test/fixtures/backup/           WebCrypto-made file, restart cases
tools/demo-core/                EmulatedDevice backup/restorePreview/restoreApply; build.sh adds the lib
web/src/lib/backup.ts           API calls, file reading/saving
web/src/hooks/useBackup.ts      card state
web/src/components/settings/BackupCard.tsx  + DeviceTab.tsx
web/src/components/settings/restart.ts      reason codes shared with the preview
web/src/demo/handlers.ts, device.ts         demo routes
web/src/mocks/handlers.ts                    MSW mocks for tests
tools/api/routes.json, operations.json, docs/api/openapi.json, docs/api/rest.md
specs/016-demo-device-emulation/contracts/schemas/backup*.schema.json
docs/user-guide/configuration.md, docs/user-guide/security.md
web/src/i18n/en.json, en.context.json, locales/*.json
```

**Structure Decision**: a new library beside `ConfigModel` (it depends on it), firmware glue in one new
`WebServer*` file, UI as one card with its hook and data module.

## Phases

1. **Library + tests**: crypto (portable + vectors), file make/read, preview, restart rules; native tests.
2. **Firmware**: mbedTLS adapter, routes, job task, staging, shared save step; both builds, flash check.
3. **Demo core**: EmulatedDevice methods, rebuild wasm, MSW demo handlers.
4. **UI**: mockups (reviewed outside the repo), then card + hook + data module; Vitest.
5. **Contract and docs**: routes/operations/schemas, OpenAPI regenerate, REST and user docs.
6. **i18n**: English + context, device catalogue, 13 translations, record, check.
7. **Gate**: native, both builds, quality check, web tests/types/lint, demo core hash.

## Complexity Tracking

None.
