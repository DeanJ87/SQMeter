# Research: Translations (spec 023)

## D1 Message format
**Decision**: flat dotted keys. A value is a string with `{name}` placeholders, or a plural object keyed by CLDR category (`zero`, `one`, `two`, `few`, `many`, `other`) using `{count}`.
**Why**: a tiny runtime (`Intl.PluralRules` picks the category), easy to diff and review, and CI can check placeholders exactly.
**Rejected**: full ICU MessageFormat - its runtime is well over the 4 KB budget.

## D2 Loading order
**Decision**: `main.tsx` asks the device for its language and file, installs the messages, sets `<html lang dir>`, then dynamically imports the app. A language change reloads the page once the device has the file - no manual reload.
**Why**: module-level text constants translate correctly with no reactive machinery.

## D3 Scope of extraction
**Decision**: everything under `web/src` that a device user sees. Excluded: tests, mocks, type declarations, and the demo's own control panel (`web/src/demo/`), which is a visitor tool around the device UI and stays English. The device UI inside the demo is translated.

## D4 Device message IDs (FR-008)
**Decision**: the `device.*` keys of `en.json` are the catalogue of device-generated text. `tools/i18n/gen_device_catalog.py` generates `lib/Messages/include/MessageCatalog.h`; `Messages::describe(text)` matches a device string against the templates (literal segments and `{params}`) and returns the key and parameter values. Response builders add `errorId`/`errorParams` beside `error`, `reasonMessages` beside `reasons`, and IDs on alert history entries when their text is a default template. English fields are unchanged.
**Why**: no change to the dozens of emitters. `gen_device_catalog.py --check` fails when an emitted literal has no template, so the catalogue can't fall behind.
**Note**: alerts *sent* by the device stay English (FR-016). The history the UI shows is translated when the text is a default template; custom wording is shown as written.

## D5 File format and source
**Decision**: `sqmeter-i18n-<code>.json.gz` is gzip of `{"lang","version","messages"}`. `sqmeter-i18n-manifest.json` lists code, file, size and SHA-256, plus the version. URL: `https://github.com/DeanJ87/SQMeter/releases/download/v<FIRMWARE_VERSION>/<asset>`. The download reuses the OTA HTTPS stream (pinned CA, redirects, TlsLock), refuses while OTA runs, checks free space (size + 4 KB), streams to `/lang.tmp` while hashing, verifies size and SHA-256, then renames to `/lang.json.gz`.
**Manual upload**: the device checks gzip magic, size ≤ 64 KB and free space; the browser validates the content (language code, messages object) before uploading and again when loading.

## D6 Restore after updates (FR-014)
**Decision**: once WiFi is up after boot, if `language != en` and the stored file is missing or its version differs from the firmware, the device starts one download (state `restoring`). A failure leaves English, or the older file with per-key fallback, in use.

## D7 Register
Recorded per language in `tools/i18n/glossary/<code>.json` (`register`). Chosen to match current device and app UIs:
es tú; fr vous; de Sie; it tu; nl je; pt-BR você; pl impersonal forms and infinitives for actions; tr siz; id Anda (formal-neutral); ar Modern Standard Arabic, formal; ja です/ます for sentences, nouns for buttons; ko 합니다/해요 polite, nouns for buttons; zh-Hans neutral, no 您 except where direct address is needed.

## D8 Review pass
**Decision**: `tools/i18n/check.mjs --review` flags glossary misses, likely untranslated English, and lengths over the context limit or over 1.6× English for tight UI strings. Flagged items are fixed by hand. The safety-critical strings are back-translated and recorded with the verdict in `tools/i18n/review/<code>.md`.

## D9 Right-to-left
**Decision**: `dir="rtl"` on `<html>` for Arabic; CSS uses logical properties; readings, units, coordinates, times and charts are isolated LTR (`dir="ltr"` / `unicode-bidi: isolate`); directional chevrons mirror under `[dir="rtl"]`.

## D10 Hard-coded text rule (I18N-01)
**Decision**: `tools/i18n/literals.mjs` parses `web/src` with the TypeScript compiler API and flags JSX text, user-facing JSX attributes (label, title, hint, placeholder, aria-label, alt, ...) and prose-like string literals outside `t()`. `tools/quality` runs it as rule I18N-01 with no baseline. Suppression: `// i18n-ignore: <reason>` (EXC-01).

## D11 Settings dependency (spec 020)
**Decision**: no catalogue entry. The language setting is never inactive: without internet it still applies through manual upload, and a failed download is reported as the language's status, not as an inactive setting.

## D12 Translation tool
**Decision**: `tools/i18n/translate.py` (standard library only) sends, per language, the new and changed keys with their context notes, the glossary and the register to the Anthropic Messages API (`claude-sonnet-5`), then makes a second review call on the result (fluency, glossary, length, back-translation of safety strings), validates with `check.mjs`, and only then writes. `record.json` stores a hash of the English each key was translated from.

## D14 Numbers: input, grouping and digits (FR-017)
**Decision**: Every number shown or typed in the UI goes through `web/src/i18n/format.ts` and `web/src/i18n/parse.ts`; ESLint rule I18N-05 bans `toFixed`, `toPrecision`, `toLocale*String`, `parseFloat` and `parseInt` in components and the demo (SVG geometry uses `web/src/lib/svg.ts`, select option values `Number(value)`).
- **Digits**: Latin in every language (`<lang>-u-nu-latn`). Readings, units and coordinates sit in left-to-right runs, the device, MQTT, Alpaca and the docs use Latin digits, and the Arabic glossary already asks for them; Arabic-Indic digits next to Latin units would read inconsistently. Typed Arabic-Indic and Persian digits are still accepted.
- **Typed numbers**: setting fields are text inputs (`type="number"` follows the browser's locale, not the app's, and silently drops "21,5" in some browsers). The language's decimal separator is accepted and the other as a fallback; grouping only in whole groups of three; a lone fallback separator before exactly three digits ("1.234" in German, "1,234" in English) is ambiguous and rejected unless the whole part is 0; anything else unreadable is kept on screen with a message, never truncated. iOS decimal keypads have no minus sign, so fields that allow negatives get the full keyboard.
- **Grouping**: readings are never grouped (21,48 not 2.148,0); counts and sizes are (65.535 in German, 65 535 in French).
**Alternatives**: keeping `type="number"` (browser-locale dependent, no control over messages); Arabic-Indic digits for Arabic (inconsistent with units, MQTT and the device).

## D13 Initial translation
The committed translations were written for this feature by an AI (Claude) acting as a native-speaker UI translator for each language, from the glossaries and context notes, then reviewed with D8. Native speakers can improve them through ordinary PRs.
