# Review note: Nederlands (nl)

Register: je/jij (informal), as current Dutch apps and devices use; buttons as infinitives ("Opslaan", "Herstarten"). Sentence case.

## Safety strings, back-translated

| Key | English | Translation | Back-translation |
|---|---|---|---|
| `safetyCard.safe` | Safe | Veilig | Safe |
| `safetyCard.unsafe` | Unsafe | Onveilig | Unsafe |
| `device.alert.observatoryUnsafe` | Observatory UNSAFE | Sterrenwacht ONVEILIG | Observatory UNSAFE |
| `settings.safety.unsafeWhileRaining` | Unsafe while raining | Onveilig bij regen | Unsafe in rain |
| `device.safety.rainDetected` | Rain detected | Regen gedetecteerd | Rain detected |
| `safetyCard.alertsPaused` | Alerts paused | Meldingen gepauzeerd | Alerts paused |

Each back-translation keeps the meaning "not safe for observing"; wording that read as "dangerous" was replaced where the language has a plain "not safe" form.

## Review findings

- Review changed settings.alerts.userKey from the English "User key" to "Gebruikerssleutel".
- Cognates kept where Dutch UIs use them (Wind, Type, Live, Dashboard, Updates, Namespaces, interrupt).
- All 1174 keys present; placeholders, plural categories and leading/trailing spaces match English (node tools/i18n/check.mjs).
- Known review false positive: settings.alerts.varEvent keeps the event identifier "unsafe" untranslated on purpose (it is the value scripts receive).
- Length flags from the context notes ("max N chars") are hints; the Playwright text-expansion check (web/tests/i18n.spec.ts) is what decides overflow at 320 px and 1280 px.
