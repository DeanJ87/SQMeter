# Review note: Português (Brasil) (pt-BR)

Register: você, as Brazilian apps use; imperatives in the você form ("Ative", "Salve"). Sentence case. Brazilian spelling and vocabulary ("tela", "arquivo", "configurações").

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Seguro | Safe |
| `safetyCard.unsafe` | Unsafe | Inseguro | Unsafe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Observatório INSEGURO | Observatory UNSAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Inseguro enquanto chove | Unsafe while it rains |
| `device.safety.rainDetected` | Rain detected | Chuva detectada | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Alertas pausados | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Brazilian usage (app, tela, você implied by imperative); "reinício" (noun) and "reiniciar" (verb) both used for restart.
- One review flag accepted: pausedTheImagingAppDisconnected says "Os alertas voltam" (alerts come back), a natural synonym of "retomar".
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
