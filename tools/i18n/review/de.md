# Review note: Deutsch (de)

Register: Sie, as most German device interfaces and technical products use; buttons as infinitives ("Speichern", "Neu starten"). Nouns capitalised as German requires.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Sicher | Safe |
| `safetyCard.unsafe` | Unsafe | Unsicher | Unsafe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Sternwarte UNSICHER | Observatory UNSAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Unsicher bei Regen | Unsafe in rain |
| `device.safety.rainDetected` | Rain detected | Regen erkannt | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Warnungen pausiert | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Formal "Sie" throughout. Cognates kept where German UIs use them (Firmware, Server, Port, Status, Wind, Name, Version, Downgrade, Updates).
- Pushover keeps its own term "User Key" because that is what the Pushover dashboard shows.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
