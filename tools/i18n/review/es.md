# Review note: Español (es)

Register: tú (informal singular), as in current device and app interfaces; imperatives in tú form ("Activa", "Guarda"). Sentence case. Use neutral international Spanish ("computadora" avoided: say "dispositivo"/"equipo").

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Seguro | Safe |
| `safetyCard.unsafe` | Unsafe | Inseguro | Unsafe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Observatorio INSEGURO | Observatory UNSAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Inseguro mientras llueve | Unsafe while it rains |
| `device.safety.rainDetected` | Rain detected | Lluvia detectada | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Alertas en pausa | Alerts on pause |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- tú register with neutral international vocabulary, so Spain and Latin American readers both read it naturally.
- Sky-quality and astronomy terms follow Spanish amateur usage (crepúsculo náutico/astronómico, gibosa menguante).
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
