# Review note: Français (fr)

Register: vous, as French device and app interfaces use; infinitive for buttons ("Enregistrer", "Redémarrer"). Sentence case. French typography: non-breaking space before : ; ? ! and %, guillemets « » for quotes.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Sûr | Safe |
| `safetyCard.unsafe` | Unsafe | Non sûr | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Observatoire NON SÛR | Observatory NOT SAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Non sûr pendant la pluie | Not safe during rain |
| `device.safety.rainDetected` | Rain detected | Pluie détectée | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Alertes suspendues | Alerts suspended |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed "unsafe" from "dangereux" (dangerous) to "non sûr" (not safe) in 23 strings: the verdict means conditions are not safe for observing, not that something is dangerous. Glossary updated.
- Typographic non-breaking spaces before : ; ? ! % » are applied by the build, as French typography requires.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
