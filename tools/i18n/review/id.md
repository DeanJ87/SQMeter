# Review note: Bahasa Indonesia (id)

Register: Formal-neutral with "Anda" where direct address is needed, as Indonesian software uses; buttons as base verbs ("Simpan", "Mulai ulang"). Sentence case.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Aman | Safe |
| `safetyCard.unsafe` | Unsafe | Tidak aman | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Observatorium TIDAK AMAN | Observatory NOT SAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Tidak aman saat hujan | Not safe when raining |
| `device.safety.rainDetected` | Rain detected | Hujan terdeteksi | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Peringatan dijeda | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Formal-neutral register; "Anda" only where direct address is needed. No plural inflection.
- Cognates kept where Indonesian UIs use them (Server, Port, Normal, Firmware, Level, File, Anemometer).
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
