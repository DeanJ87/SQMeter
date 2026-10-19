# Tasks: Settings backup and restore

**Input**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md),
[contracts/](contracts/), [quickstart.md](quickstart.md)

Tests are required by constitution III (native tests for `lib/`, Vitest for the UI) and by the spec's
success criteria SC-002, SC-003, SC-005.

## Phase 1: Setup

- [x] T001 Create `lib/ConfigBackup/{include,src}` and add `-Ilib/ConfigBackup/include` to `[env:native]` in platformio.ini
- [x] T002 [P] Add `lib/ConfigBackup/src/*.cpp` to the sources in tools/demo-core/build.sh

## Phase 2: Foundational (blocks every story)

- [x] T003 Declare `Backup::deriveKey`, `Backup::seal`, `Backup::open` and their sizes in lib/ConfigBackup/include/BackupCrypto.h
- [x] T004 Implement portable SHA-256, HMAC-SHA256, PBKDF2, AES-256 and GCM behind `#ifndef ESP_PLATFORM` in lib/ConfigBackup/src/PortableCrypto.cpp
- [x] T005 Implement the mbedTLS adapter (PBKDF2 via `mbedtls_pkcs5_pbkdf2_hmac_ext`, GCM via `mbedtls_gcm_*`) in src/BackupCryptoMbed.cpp
- [x] T006 Native crypto tests against FIPS 180-4, RFC 4231, RFC 7914 and NIST GCM vectors in test/test_config_backup/test_main.cpp
- [x] T007 Declare the public API (constants, `DeviceInfo`, `BoardPins`, `Parsed`, `Preview`, error codes, `makeBackup`, `parseBackup`, `openSecrets`, `buildPreview`, `previewJson`, `restartReasons`, `compareVersions`) in lib/ConfigBackup/include/ConfigBackup.h
- [x] T008 Implement restart reasons (same rules as restart.ts), version compare and the classic ESP32 pin set in lib/ConfigBackup/src/BackupRules.cpp
- [x] T009 [P] Shared restart fixture test/fixtures/backup/restart-cases.json, run by the native tests and web/src/components/settings/__tests__/restart.test.ts
- [x] T010 Refactor web/src/components/settings/restart.ts to name each reason by the same code (`wifi`, `ota`, `gps`, `i2c`, `ipv6`, `alpaca`, `ble`) and export the code-to-label map
- [x] T011 Extract the save-and-apply step of `WebServer::handleSetConfig` into `WebServer::saveAndApply` in src/WebServerApi.cpp and include/WebServer.h

## Phase 3: User Story 1 - Back up the settings (P1)

**Goal**: a file with every setting, no clear-text secret, optional encrypted secrets.
**Independent test**: back up without a passphrase; the file has every section and no secret value.

- [x] T012 [US1] Implement `makeBackup` (header, `toJson(true)` settings, secrets plaintext, salt/nonce from the caller, base64) in lib/ConfigBackup/src/BackupFile.cpp
- [x] T013 [US1] Native tests: no secret in a passphrase-less backup (SC-002), every section present, header fields, secrets block shape, round trip with the right passphrase in test/test_config_backup/test_main.cpp
- [x] T014 [US1] `POST /api/backup`: raw body (≤ 20,480 bytes), `requireAuth`, passphrase length, background job pinned to core 1, one job at a time (no `Content-Disposition`: flash, research R7), derivation time logged in src/WebServerBackup.cpp, include/WebServer.h, src/WebServer.cpp (route setup)
- [x] T015 [P] [US1] Demo: `EmulatedDevice::backup` in tools/demo-core/emulated_device_backup.cpp, binding in tools/demo-core/bridge.cpp, handler in web/src/demo/handlers.ts and web/src/demo/device.ts
- [x] T016 [P] [US1] Data module: `requestBackup`, `saveBackupFile` (pretty-print, file name) in web/src/lib/backup.ts

## Phase 4: User Story 2 - Restore with a preview (P1)

**Goal**: upload, preview grouped by section, apply exactly what was previewed.
**Independent test**: restore onto a device with different settings; the preview lists exactly the differences; applying makes the settings match.

- [x] T017 [US2] Implement `parseBackup` (format detection, format 0, damaged/not-backup/newer-format, size) in lib/ConfigBackup/src/BackupFile.cpp
- [x] T018 [US2] Implement `buildPreview` (candidate via `Config::applyJson` with placeholders kept, secret status, restart) and `previewJson` (masked candidate settings) in lib/ConfigBackup/src/BackupPreview.cpp; the per-setting changes, kept/ignored paths and warnings in web/src/lib/backupPreview.ts (research R10)
- [x] T019 [US2] Native tests: restored setting saved, unchanged backup → candidate equals current, validation failure → `invalid` with the save's message, restart codes, preview document shape (test/test_config_backup/test_main.cpp); Vitest: changes listed exactly, kept/ignored, warnings (WiFi network, device password, address, imaging app, newer firmware) in web/src/lib/__tests__/backupPreview.test.ts
- [x] T020 [US2] `POST /api/backup/preview` (job, staged candidate + token + revision, 10-minute expiry) and `POST /api/backup/apply` (token, revision check, `saveAndApply`, restart when needed) in src/WebServerBackup.cpp
- [x] T021 [P] [US2] Demo: `restorePreview`, `restoreApply` in tools/demo-core/emulated_device_backup.cpp and bridge.cpp; handlers in web/src/demo/handlers.ts, web/src/demo/device.ts (restart after apply)
- [x] T022 [US2] Data module `readBackupFile` (64 KB limit, JSON parse, header), `previewRestore`, `applyRestore` in web/src/lib/backup.ts; state hook web/src/hooks/useBackup.ts
- [x] T023 [US2] Mockups of every card state (desktop 1280 px, phone 390 px). The mockup was reviewed outside the repo; mockup images are not committed
- [x] T024 [US2] `BackupCard` per contracts/ui.md in web/src/components/settings/BackupCard.tsx; add it to web/src/components/settings/DeviceTab.tsx
- [x] T025 [US2] MSW mocks for the three routes in web/src/mocks/handlers.ts; Vitest web/src/components/__tests__/BackupCard.test.tsx (backup, preview, apply, cancel, refusal)

## Phase 5: User Story 3 - Secrets only with the passphrase (P1)

**Goal**: right passphrase restores secrets, wrong one changes nothing, none keeps this device's secrets.
**Independent test**: restore an encrypted backup with right, wrong and no passphrase.

- [x] T026 [US3] Implement `openSecrets` (iteration bounds, tag check, plaintext parse) and the secret status in the preview (`restored`/`kept`/`none`, changed/unchanged paths, `pushover-keys`) in lib/ConfigBackup/src/BackupFile.cpp and BackupPreview.cpp
- [x] T027 [US3] Native tests: wrong passphrase, changed ciphertext/tag/nonce/salt → `wrong-passphrase` and no candidate; no passphrase → secrets kept; secret cleared when the backup's is empty; WebCrypto-made fixture opens (test/fixtures/backup/webcrypto-backup.json) in test/test_config_backup/test_main.cpp
- [x] T028 [P] [US3] Generate the fixture with Node's WebCrypto: test/fixtures/backup/make-webcrypto-fixture.mjs

## Phase 6: User Story 4 - Safe across boards and versions (P2)

**Goal**: pins and features this board lacks aren't applied; older and newer files handled.
**Independent test**: a backup with pins this board doesn't have lists them as not applicable and restores the rest.

- [x] T029 [US4] Not-applicable pass (pins, analog pin, Bluetooth on the standard build) on the backup document before applying in lib/ConfigBackup/src/BackupPreview.cpp
- [x] T030 [US4] Native tests: cross-board pins (an ESP32-S3-like `BoardPins`), Bluetooth on standard, Pushover without keys, format 0, newer format, damaged files, a body over 20,480 bytes refused before parsing (test/test_config_backup); older firmware (kept) and newer firmware (ignored + warning) in Vitest (T019)
- [x] T038 [US4] Board pins from the build's board profile (`Backup::boardPins`, `buildBoardPins`, spec 034) instead of the classic ESP32 set, in the firmware and the demo; the I2C-2, display button and battery pins are checked too, with 255 (not wired) kept; native test with the ESP32-S3-ETH profile

## Phase 7: Polish & cross-cutting

- [x] T031 [P] Routes in tools/api/routes.json, operations in tools/api/operations.json, schemas backup.schema.json and backup-preview.schema.json in specs/016-demo-device-emulation/contracts/schemas/; regenerate docs/api/openapi.json (`python3 tools/api/openapi.py`, `--check`)
- [x] T032 [P] REST docs for the three routes in docs/api/rest.md; replace "Backup & Restore" in docs/user-guide/configuration.md; passphrase note in docs/user-guide/security.md
- [x] T033 i18n: English strings and context notes (web/src/i18n/en.json, en.context.json); device messages (`python3 tools/i18n/gen_device_catalog.py`, adding lib/ConfigBackup/src/BackupFile.cpp and BackupPreview.cpp to its sources); 13 translations in web/src/i18n/locales/*.json; `python3 tools/i18n/translate.py --all --record`; `node tools/i18n/check.mjs`
- [x] T034 Rebuild the demo core (tools/demo-core/build.sh) and check `python3 tools/demo-core/source_hash.py --check`
- [x] T035 Gate: `pio test -e native`, `pio run -e esp32dev`, `pio run -e esp32dev-ble` (record flash in tools/firmware/size-baseline.json if it moves > 2 KB), `tools/quality/check.py`, web vitest/tsc/eslint, UI size baseline
- [x] T036 [P] CHANGELOG.md entry under the unreleased section
- [ ] T037 On the spare device (owner, before release): both builds, `freeHeap` and `stackFree` during a backup and a preview, the logged derivation time, and SC-001 (restore onto a reset device) - constitution IV and the device check in Development Workflow

## Dependencies

- Setup → Foundational → US1 → US2 → US3 → US4 → Polish. US3 and US4 extend the library and tests built in US1/US2
  (same files), so they follow in order; the demo (T015, T021) and the data module (T016) can run beside the firmware tasks.
- T023 (mockups) precedes T024 (card).

## Parallel examples

- US1: T015 (demo) and T016 (web data module) beside T014 (firmware).
- Polish: T031, T032, T036 together.

## Implementation strategy

MVP = US1 + US2 (backup and preview/apply without secrets); US3 adds the secrets; US4 the cross-board rules.
All four ship in one PR.

## Gate (2026-10-18)

- Native: 314 tests pass (`pio test -e native`); both firmware builds pass with no warnings from the new code.
- Flash: +24.2 KB standard (84.7% of the slot), +24.0 KB Bluetooth (95.3%: over the 95% line; reason
  recorded in tools/firmware/size-baseline.json for the owner to accept or reject).
- After merging specs 030-032 and 034 (2026-10-19): every build under 95% with no waiver (C++ exceptions
  off; OLED and battery drivers only in the S3 builds); see [budgets.md](budgets.md).
- `tools/quality/check.py`: 0 findings. Web: 645 Vitest tests, `tsc`, ESLint clean; UI +4.4 KB gzipped
  (limit +10 KB). OpenAPI, routes, i18n (14 languages), copy, labels, dashboard inventory, demo core
  hash: OK. `mkdocs build --strict`: only the CI-generated screenshots are missing (as on main).

## Analysis (speckit-analyze, 2026-10-18)

| ID | Category | Severity | Finding | Resolution |
|---|---|---|---|---|
| A1 | Constitution IV | HIGH | Heap/stack use must be measured on a device; this work may not touch a device. | T037 left for the owner on the spare device; research R7 gives the expected figures. |
| A2 | Workflow | HIGH | UI must follow an owner-reviewed mockup. | T023 before T024; the mockup was reviewed outside the repo, with any difference between build and mockup. |
| A3 | Coverage | MEDIUM | "File over the limit refused before parsing" had no test. | Added to T030. |
| A4 | Coverage | MEDIUM | SC-001 needs real WiFi/MQTT. | T037. |
| A5 | Inconsistency | LOW | T028 path. | Fixed. |

No CRITICAL findings. Every FR maps to at least one task (FR-001 T012/T024, FR-002 T012, FR-003 T013,
FR-004 T004/T005/T012, FR-005 T014, FR-006 T022/T024, FR-007 T018/T026/T029, FR-008 T018, FR-009 T020,
FR-010 T017/T026/T030, FR-011 T026, FR-012 T029, FR-013 T020, FR-014 T015/T021/T031, FR-015 T032,
FR-016 T033, FR-017 T014).
