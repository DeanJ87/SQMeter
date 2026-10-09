# Review note: Italiano (it)

Register: tu, as current Italian apps and devices use; imperatives in tu form ("Attiva", "Salva"). Sentence case.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Sicuro | Safe |
| `safetyCard.unsafe` | Unsafe | Non sicuro | Not safe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Osservatorio NON SICURO | Observatory NOT SAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Non sicuro mentre piove | Not safe while it rains |
| `device.safety.rainDetected` | Rain detected | Pioggia rilevata | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Avvisi sospesi | Alerts suspended |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed safetyCard.unsafeReasons from "Motivi del pericolo" (reasons for the danger) to "Perché non è sicuro" (why it is not safe), and translated the watchdog reasons.
- "Password", "File" and "DIP switch" are kept as Italian UIs write them.
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
