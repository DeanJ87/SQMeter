# Review note: Polski (pl)

Register: Impersonal and infinitive forms, as Polish software uses, avoiding direct pan/pani or ty where possible ("Zapisz" for buttons is the accepted imperative). Sentence case.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Bezpiecznie | Safe |
| `safetyCard.unsafe` | Unsafe | Niebezpiecznie | Unsafe (niebezpiecznie is the plain antonym of bezpiecznie; it can also read as "dangerous") |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Obserwatorium NIEBEZPIECZNE | Observatory UNSAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Niebezpiecznie podczas deszczu | Unsafe during rain |
| `device.safety.rainDetected` | Rain detected | Wykryto deszcz | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Powiadomienia wstrzymane | Notifications held |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed two strings that said "zagrożenie" (threat) to the plain "niebezpiecznie" (unsafe), the usual antonym of "bezpiecznie" in Polish UIs.
- Firmware is "oprogramowanie" as Polish device UIs write it; the glossary now says so (it said "oprogramowanie układowe", rare in device UIs).
- Plurals use one/few/many/other; "many" covers 5-21, 25-31 and so on.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
