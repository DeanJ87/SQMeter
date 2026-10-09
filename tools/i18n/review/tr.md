# Review note: Türkçe (tr)

Register: siz (polite plural), as Turkish software uses; buttons as short verb stems or nouns ("Kaydet", "Yeniden başlat"). Sentence case.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Güvenli | Safe |
| `safetyCard.unsafe` | Unsafe | Güvenli değil | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Gözlemevi GÜVENLİ DEĞİL | Observatory NOT SAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Yağmur yağarken güvenli değil | Not safe while it is raining |
| `device.safety.rainDetected` | Rain detected | Yağmur algılandı | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Uyarılar duraklatıldı | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Placeholders stand alone or before a colon ("Sensör arızası: {name}") so Turkish suffixes never attach to a placeholder.
- Glossary lists "eşik|eşiğ" because of consonant mutation (eşik -> eşiği); "Test" is the Turkish word too.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
