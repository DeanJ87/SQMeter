# Review note: 한국어 (ko)

Register: Polite 합니다/해요 style for sentences (합니다 for status and errors, consistent throughout); buttons and labels as nouns or short forms ("저장", "다시 시작").

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | 안전 | Safe |
| `safetyCard.unsafe` | Unsafe | 안전하지 않음 | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | 관측소 안전하지 않음 | Observatory not safe |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | 비가 오는 동안 안전하지 않음 | Not safe while it rains |
| `device.safety.rainDetected` | Rain detected | 비 감지됨 | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | 알림 일시 중지 | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed "unsafe" from "위험" (danger) to "안전하지 않음" (not safe) in the glossary before translation.
- 합니다 style for status and errors, short nouns for labels. Compass points use 북/동/남/서.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
