# Review note: العربية (ar)

Register: Modern Standard Arabic, formal, as Arabic software uses; masculine-neutral imperatives for buttons ("احفظ", "أعد التشغيل") or verbal nouns ("حفظ"). Right-to-left.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | آمن | Safe |
| `safetyCard.unsafe` | Unsafe | غير آمن | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | المرصد غير آمن | The observatory is not safe |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | غير آمن أثناء المطر | Not safe during rain |
| `device.safety.rainDetected` | Rain detected | اكتُشف مطر | Rain was detected |
| `safetyCard.alertsPaused` | Alerts paused | التنبيهات متوقفة مؤقتًا | Alerts temporarily stopped |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Right-to-left. Forward arrows between Arabic words are mirrored (←); arrows inside Latin runs such as "OUT → GPIO 16" stay as they are because the UI isolates them left-to-right.
- Plurals use all six categories (zero, one, two, few, many, other); one and two are written out as words, as Arabic does.
- Numbers stay as Western digits, filled in by the UI; the Arabic comma "،" is used in prose.
- Glossary lists alternative forms because Arabic attaches clitics and uses verb forms of the same root (استئناف / تستأنف).
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
