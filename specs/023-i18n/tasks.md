# Tasks: Translations (spec 023)

## Phase 1: Setup
- [x] T001 Create `web/src/i18n/` (`index.ts`, `languages.ts`) and the `tools/i18n/` skeleton
- [x] T002 [P] Runtime `t()` with placeholders, `Intl.PluralRules` plurals and locale-aware formatting helpers in web/src/i18n/index.ts (+ tests)

## Phase 2: Foundational
- [x] T003 Extract every user-facing string in web/src (excluding the demo panel, tests, mocks) into web/src/i18n/en.json and replace it with `t()`
- [x] T004 Fix plurals, concatenations and sentences the extraction split
- [x] T005 Bootstrap in web/src/main.tsx: load the language, set `<html lang dir>`, then import the app
- [x] T006 [P] Rule I18N-01 `tools/i18n/literals.mjs`, wired into tools/quality (no baseline) and the coding standard
- [x] T007 [P] A context note for every key in web/src/i18n/en.context.json

## Phase 3: US1 - Use SQMeter in another language
- [x] T008 [US1] `language` in lib/ConfigModel (default `en`, validated against lib/LanguageLogic codes, JSON round trip, tests)
- [x] T009 [US1] lib/LanguageLogic: codes, manifest parsing, file checks (+ native tests)
- [x] T010 [US1] src/LanguagePack: download task (OTA stream, TlsLock, OTA exclusion, free space, SHA-256, atomic rename), delete, status
- [x] T011 [US1] Routes `/api/i18n`, `/api/i18n/install`, `/api/i18n/upload` (the file is served at `/lang.json`; `/api/i18n/pack` dropped for flash); config change hooks
- [x] T012 [US1] Settings → Device → Language with status, retry and upload; reload once installed
- [x] T013 [US1] Device message IDs: `device.*` keys, gen_device_catalog.py → `device.*` templates in en.json, UI `deviceText()`/`deviceError()` recognise device text (no response fields: flash budget, research D4)
- [x] T014 [US1] FR-017 numbers and dates follow the language

## Phase 4: US2 - English always works
- [x] T015 [US2] Loader fallbacks: no file, bad file, per-key missing; a notice with the reason and retry/upload (+ tests)
- [x] T016 [US2] Restore after a firmware or filesystem update (boot hook)

## Phase 5: US3 - Every string translated
- [x] T017 [US3] Glossaries for the 13 languages (tools/i18n/glossary)
- [x] T018 [US3] Translations: id, es, fr, it, de, nl, ar, pt-BR, pl, ja, zh-Hans, ko, tr
- [x] T019 [US3] Review pass: `check.mjs --review`, fixes, back-translation notes in tools/i18n/review/<code>.md
- [x] T020 [US3] tools/i18n/check.mjs (completeness, extra keys, placeholders, plurals, JSON validity, glossary, length) + CI
- [x] T021 [US3] tools/i18n/translate.py (Anthropic API, changed keys only via record.json, context + glossary + review pass) + tests
- [x] T022 [US3] tools/i18n/build_packs.py; release workflow publishes the files and the manifest

## Phase 6: US4 - Offline devices and the demo
- [x] T023 [US4] Manual upload end to end (browser validation + device checks)
- [x] T024 [US4] Demo: emulated `/api/i18n*` serving the files from the demo site; nothing outbound

## Phase 7: Right-to-left and layout
- [x] T025 Logical CSS properties in web/src/index.css; LTR isolation for values, units and charts; mirrored chevrons
- [x] T026 Playwright per-language overflow check at 320 and 1280 px; Arabic right-to-left and axe check

## Phase 8: Polish
- [x] T027 Docs: docs/user-guide/languages.md, docs/development/translations.md, mkdocs nav
- [x] T028 Size budgets measured (file sizes, runtime gzip, firmware delta) and recorded
- [x] T029 Contract schema for /api/i18n; contract-check
- [x] T030 Converge

## Converge (2026-10-09)

Every FR and SC checked against the code. Converge found, and this branch fixed:

- Untranslated text the literal check missed: single words in template literals (`rises ${clock}`, `${n} samples`, ` at ${time}`, `a and b`), the `raw` unit, the update button's `to <tag>`, the BLE `connected` badge. I18N-01 now flags a word beside a template value.
- Device descriptions shown as-is (cloud condition, Bortle class): added to the device catalogue and shown through `deviceText()`.
- Labels built when a module loads stayed English in the demo (`SKY_PHASE_LABEL`, netAddress messages): now translated when read (I18N-04).
- `language` was dropped by the settings payload, so the form showed English and a change couldn't be saved.
- Arabic: dashboard cards in left-to-right order, mirrored units, unmirrored Masonry arrows and tab arrow keys.
- Text expansion at 320 px: the settings tab strip and form-grid selects widened the page (es, fr, it, pl); buttons now wrap on phones.
- The imaging-app status doubled "ago" in every language.
- French, Italian, Polish, Japanese and Korean said "dangerous" for "unsafe": now the literal "not safe" (review notes).
- `/api/i18n*` routes were invisible to the route registry (they live in LanguagePack.cpp); the registry now reads that file too.

Converged: no FR or SC left open in code. The Playwright check (every page, every language, 320 and 1280 px, English-leftover detection, Arabic axe) passes for all 13 languages.

Device checks (need hardware, not run here): choosing Español installs within 10 s and `GET /api/i18n` shows `installed`; switching to English deletes `/lang.json.gz`; manual upload works with the device offline; the language comes back after an OTA update (`restoring`); contract check against a device; free heap during a download.
