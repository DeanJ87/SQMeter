# Review note: 简体中文 (zh-Hans)

Register: Neutral and concise, as mainland Chinese software uses; avoid 您/你 unless the sentence needs direct address (then 你). Buttons as short verbs ("保存", "重启"). Full-width punctuation ，。：（）“”.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | 安全 | Safe |
| `safetyCard.unsafe` | Unsafe | 不安全 | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | 天文台不安全 | Observatory not safe |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | 下雨时视为不安全 | Treated as not safe when raining |
| `device.safety.rainDetected` | Rain detected | 检测到降雨 | Rainfall detected |
| `safetyCard.alertsPaused` | Alerts paused | 警报已暂停 | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Neutral mainland usage; "你" only where direct address is needed. No spaces between Chinese and Latin text.
- Chinese has no spaces between words, so the check does not require English edge spaces here.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
