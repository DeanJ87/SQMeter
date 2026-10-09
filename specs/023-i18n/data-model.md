# Data Model: Translations

- **English source** `web/src/i18n/en.json`: `{ "<key>": "text {param}" | { "one": "...", "other": "..." } }`. Keys are `<area>.<name>`. `device.*` keys are device-generated text (the catalogue source).
- **Context** `web/src/i18n/en.context.json`: `{ "<key>": "note[; max N chars]" }` - exactly the keys of `en.json`.
- **Language file** `web/src/i18n/locales/<code>.json`: the same keys. Plural objects carry every CLDR category the language needs (ar: zero one two few many other; pl: one few many other; id, ja, ko, zh-Hans: other).
- **Glossary** `tools/i18n/glossary/<code>.json`: `{ "register": "...", "keep": ["SQM", ...], "terms": { "sky quality": "...", ... } }`.
- **Review note** `tools/i18n/review/<code>.md`: checks run, fixes made, back-translations of the safety strings.
- **Translation record** `tools/i18n/record.json`: `{ "english": { "<key>": "<sha1(english)[:10]>" }, "languages": { "<code>": { "<key>": "<hash or -" } } }`. `english` is the English every language was last brought up to; a language lists only the keys it was translated from different English (`-` = never translated), so the file stays small (about 60 KB instead of 780 KB).
- **File** (release asset and device file): gzip of `{ "lang": "es", "version": "0.2.1", "messages": { ... } }`, at most 64 KB compressed.
- **Manifest** (release asset): `{ "version": "0.2.1", "languages": [ { "code", "file", "size", "sha256" } ] }`.
- **Device config**: `language` - `en` or a supported code; default `en`.
- **Device language state**: `idle | downloading | installed | failed | restoring`, the last error (with `errorId`), and the installed file's `{lang, version, size}`.
