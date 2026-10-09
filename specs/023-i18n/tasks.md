# Tasks: Translations (spec 023)

## Phase 1: Setup
- [ ] T001 Create `web/src/i18n/` (`index.ts`, `languages.ts`) and the `tools/i18n/` skeleton
- [ ] T002 [P] Runtime `t()` with placeholders, `Intl.PluralRules` plurals and locale-aware formatting helpers in web/src/i18n/index.ts (+ tests)

## Phase 2: Foundational
- [ ] T003 Extract every user-facing string in web/src (excluding the demo panel, tests, mocks) into web/src/i18n/en.json and replace it with `t()`
- [ ] T004 Fix plurals, concatenations and sentences the extraction split
- [ ] T005 Bootstrap in web/src/main.tsx: load the language, set `<html lang dir>`, then import the app
- [ ] T006 [P] Rule I18N-01 `tools/i18n/literals.mjs`, wired into tools/quality (no baseline) and the coding standard
- [ ] T007 [P] A context note for every key in web/src/i18n/en.context.json

## Phase 3: US1 - Use SQMeter in another language
- [ ] T008 [US1] `language` in lib/ConfigModel (default `en`, validated against lib/LanguageLogic codes, JSON round trip, tests)
- [ ] T009 [US1] lib/LanguageLogic: codes, manifest parsing, file checks (+ native tests)
- [ ] T010 [US1] src/LanguagePack: download task (OTA stream, TlsLock, OTA exclusion, free space, SHA-256, atomic rename), delete, status
- [ ] T011 [US1] Routes `/api/i18n`, `/api/i18n/pack`, `/api/i18n/install`, `/api/i18n/upload`; config change hooks
- [ ] T012 [US1] Settings → Device → Language with status, retry and upload; reload once installed
- [ ] T013 [US1] Device message IDs: `device.*` keys, gen_device_catalog.py → lib/Messages (describe + tests), `errorId`/`reasonMessages`/alert history IDs in responses, UI `deviceMessage()`
- [ ] T014 [US1] FR-017 numbers and dates follow the language

## Phase 4: US2 - English always works
- [ ] T015 [US2] Loader fallbacks: no file, bad file, per-key missing; a notice with the reason and retry/upload (+ tests)
- [ ] T016 [US2] Restore after a firmware or filesystem update (boot hook)

## Phase 5: US3 - Every string translated
- [ ] T017 [US3] Glossaries for the 13 languages (tools/i18n/glossary)
- [ ] T018 [US3] Translations: id, es, fr, it, de, nl, ar, pt-BR, pl, ja, zh-Hans, ko, tr
- [ ] T019 [US3] Review pass: `check.mjs --review`, fixes, back-translation notes in tools/i18n/review/<code>.md
- [ ] T020 [US3] tools/i18n/check.mjs (completeness, extra keys, placeholders, plurals, JSON validity, glossary, length) + CI
- [ ] T021 [US3] tools/i18n/translate.py (Anthropic API, changed keys only via record.json, context + glossary + review pass) + tests
- [ ] T022 [US3] tools/i18n/build_packs.py; release workflow publishes the files and the manifest

## Phase 6: US4 - Offline devices and the demo
- [ ] T023 [US4] Manual upload end to end (browser validation + device checks)
- [ ] T024 [US4] Demo: emulated `/api/i18n*` serving the files from the demo site; nothing outbound

## Phase 7: Right-to-left and layout
- [ ] T025 Logical CSS properties in web/src/index.css; LTR isolation for values, units and charts; mirrored chevrons
- [ ] T026 Playwright per-language overflow check at 320 and 1280 px; Arabic right-to-left and axe check

## Phase 8: Polish
- [ ] T027 Docs: docs/user-guide/languages.md, docs/development/translations.md, mkdocs nav
- [ ] T028 Size budgets measured (file sizes, runtime gzip, firmware delta) and recorded
- [ ] T029 Contract schema for /api/i18n; contract-check
- [ ] T030 Converge
