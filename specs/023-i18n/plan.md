# Implementation Plan: Translations (spec 023)

**Branch**: `feat/023-i18n` | **Date**: 2026-10-09 | **Spec**: [spec.md](spec.md)

## Summary

All web UI text moves into `web/src/i18n/en.json` (keyed messages, with a context note per key in `en.context.json`). Thirteen committed language files (`web/src/i18n/locales/<code>.json`) translate every key, built from per-language glossaries and checked by a review pass. The UI has English built in; when the device's `language` setting is not English it fetches the device's stored language file before the app modules load, so every module sees the right text. Device-generated text the UI shows gets a stable message ID from a catalogue generated from the `device.*` keys of the English file. The device downloads `sqmeter-i18n-<code>.json.gz` from the GitHub release matching its firmware, checks it against the release's manifest (size, SHA-256), and keeps at most one file in LittleFS.

## Technical Context

- **Languages**: C++17 (ESP32 Arduino; `lib/` pure and native-tested), TypeScript/Preact (web), Python 3 and Node (tools).
- **Dependencies**: none new at runtime. Plurals use `Intl.PluralRules`; numbers and dates use `Intl.NumberFormat`/`DateTimeFormat`. Device hashing uses mbedTLS SHA-256 (in the core). The translation tool calls the Anthropic Messages API with `ANTHROPIC_API_KEY`.
- **Storage**: the device language in the config (NVS); the file at `/lang.json.gz` in LittleFS (gzip, served with `Content-Encoding: gzip`).
- **Testing**: Unity native tests (`lib/Messages`, `lib/LanguageLogic`, ConfigModel), Vitest (runtime, loader, formatting), Playwright (every page per language at 320 px and 1280 px; Arabic right-to-left with axe), Python tests for the tools.
- **Budgets**: see the spec's Size Budgets; measured in T028 (2026-10-09, against main e9f4b09):
  - Language files: 22.2 KB (id) to 25.2 KB (ar) gzip, limit 64 KB.
  - i18n runtime (index, format, loader, deviceMessage, languages; esbuild, minified, gzip -9): 2,242 bytes, limit 4 KB.
  - Firmware: esp32dev 1,483,677 bytes vs main 1,471,757 (+11,920); esp32dev-ble 1,712,309 vs 1,700,685 (+11,624); limit +12 KB (12,288).
  - `tools/i18n/record.json`: 61 KB (one English baseline plus per-language differences).
- **Constraints**: nothing outbound from the demo (spec 016); machine-readable output unchanged (FR-015); one TLS session at a time (TlsLock); never concurrent with OTA.

## Constitution Check

| Principle | How this plan complies |
|---|---|
| I Fail-safe safety verdict | Untouched: translation is display only; safety JSON keeps its English `reasons` and adds `reasonMessages`. |
| II ASCOM Alpaca | Alpaca output is never translated (FR-015); ConformU runs in CI. |
| III Testable pure logic | Message matching, manifest parsing and language validation live in `lib/Messages` and `lib/LanguageLogic` with native tests. |
| IV Embedded budgets | File ≤ 64 KB gzip; runtime ≤ 4 KB gzip; firmware delta measured; free-space check before download; download in its own task under TlsLock. |
| V Quiet, consistent UI | One language setting in Settings → Device; errors explain and offer retry or upload. |
| VI Trusted-LAN security | Install, upload and the language change sit behind the existing auth guard. Only GitHub release URLs are fetched, with the pinned CA. |
| VII Docs move with behaviour | `docs/user-guide/languages.md`; translator guide `docs/development/translations.md`. |
| VIII Code quality | New I18N-01 rule (no hard-coded UI text) in `tools/quality`; all new code passes `check.py`. |

## Project Structure

```text
web/src/i18n/            en.json, en.context.json, locales/<code>.json, index.ts (t, plural, formatting),
                         languages.ts, loader.ts (device file fetch + validation), deviceMessage.ts
web/src/main.tsx         loads the language, sets <html lang dir>, then imports the app
lib/Messages/            generated catalogue (MessageCatalog.h) + describe(text) -> {key, params}
lib/LanguageLogic/       language codes, manifest parsing, file checks
src/LanguagePack.*       download task, upload, delete, status, restore after updates
tools/i18n/              check.mjs, literals.mjs (I18N-01), translate.py, build_packs.py,
                         gen_device_catalog.py, glossary/<code>.json, review/<code>.md, record.json
.github/workflows/       build.yml: i18n checks; files and manifest as release assets
```

## Phases

1. **Runtime and extraction** (US3 foundations): runtime, extraction of every string, I18N-01, context notes.
2. **Device** (US1, US2, US4): config `language`, `lib/Messages`, `lib/LanguageLogic`, `LanguagePack`, routes, release assets, demo.
3. **Translations** (US1, US3): glossaries, 13 language files, review pass and notes, translation tool.
4. **Right-to-left and layout** (FR-019, FR-023): logical CSS, direction isolation, per-language overflow and axe checks.
5. **Docs, budgets, convergence.**

## Complexity Tracking

| Choice | Why | Simpler alternative rejected |
|---|---|---|
| App modules imported after the language loads | Module-level labels (tab lists, option lists) translate without turning every constant into a function | A reactive i18n store with re-render: larger runtime, every module constant rewritten |
| Device catalogue generated from `en.json` | One source of truth for device text and its translations | A hand-kept C++ table drifting from the UI |
