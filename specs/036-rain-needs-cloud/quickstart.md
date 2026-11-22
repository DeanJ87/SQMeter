# Quickstart: validating Rain needs cloud

## Automated

```bash
pio test -e native -f test_rain_trust -f test_alpaca_safety -f test_device_core -f test_alert_logic
pio test -e native                     # everything
(cd web && npm run typecheck && npx vitest run)
node tools/i18n/check.mjs && python3 tools/i18n/gen_device_catalog.py --check
.venv-quality/bin/python tools/quality/check.py
mkdocs build --strict
```

`test_rain_trust` covers: setting off = old behaviour; clear + healthy = safe and dew flag; cloud at
the level = unsafe; sky sensor missing, stale, faulted or uncomputable = unsafe; the latch across a
cloud flicker; the 20 Nov 19:43 dew replay (rain 0.02-0.17 mm/h at 0% cloud) and the 22 Nov 05:49
shower replay.

## In the demo (`cd web && npm run dev:demo`)

1. Settings → Safety → Rain: turn on **Rain needs cloud** (level 10%). Save.
2. Make it rain with a clear sky: the dashboard rain card shows "Rain under a clear sky: dew
   suspected" and the Safety Monitor stays safe; `/api/safety` has `dewSuspected: true`.
3. Make it cloudy (above 10%): the verdict goes unsafe with "Rain ... with cloud ...% >= 10%".
4. Turn the setting off: rain is unsafe as before.

## On a device (not done in this PR)

OTA to the test device, enable the setting, cover the IR sensor's view with a warm object or wait
for a dewy night, and check `/api/safety` and the HA entity.
