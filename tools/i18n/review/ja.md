# Review note: 日本語 (ja)

Register: です/ます polite form for sentences; buttons and labels as nouns or short verb forms ("保存", "再起動"). No spaces between words; use full-width punctuation 。、「」（）.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | 安全 | Safe |
| `safetyCard.unsafe` | Unsafe | 安全でない | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | 観測所は安全ではありません | The observatory is not safe |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | 降雨中は安全でない | Not safe while raining |
| `device.safety.rainDetected` | Rain detected | 降雨を検出 | Rainfall detected |
| `safetyCard.alertsPaused` | Alerts paused | アラート一時停止 | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed "unsafe" from "危険" (danger) to "安全でない" (not safe) in the glossary before translation, for the same reason as French.
- です/ます for sentences, nouns for labels; full-width punctuation. Japanese has no spaces between words, so the check does not require English edge spaces (" and ", ", ") here.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
