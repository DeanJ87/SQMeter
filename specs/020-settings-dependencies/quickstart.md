# Quickstart: validating settings dependencies

## Automated (what CI runs)

```bash
pio test -e native                              # test/test_settings_deps + tagged tests in other suites
python3 tools/settings-deps/test_check.py       # the checker itself (US4)
python3 tools/settings-deps/check.py            # catalogue ↔ code ↔ tests ↔ docs
python3 tools/demo-core/source_hash.py --check  # the demo runs the same rules
cd web && npx tsc --noEmit && npx vitest run    # parity fixtures, Settings UI per catalogue ID
npm run build:demo && npx playwright test tests/settings-deps.spec.ts
```

Expected: everything passes; `check.py` prints "catalogue, code, tests and docs agree".

## By hand in the demo (or on a device)

1. **US1** - Settings → Alerts: turn on Send alerts and the MQTT channel; Settings → Network: turn MQTT off. Before saving, a note says "Saving makes these inactive: MQTT alerts (MQTT is off)". Save. Back on Alerts: "Inactive - MQTT is off" under the MQTT switch, a **Turn on MQTT** button, no **Send test**, the channels badge doesn't count it. `GET /api/settings/effective` shows `alerts.mqtt.enabled` inactive with `mqtt-off`.
2. **US2** - Switch MQTT back on and save: the MQTT channel is active again with nothing re-entered.
3. **D-15** - Settings → Sensors: rain sensor off; Safety: "Unsafe while raining" shows "Not in effect - Rain sensor is off"; `GET /api/safety` lists it in `rulesNotInEffect`.
4. **US4** - Add `if (cfg.ota.enabled && cfg.rain.enabled)` to any `src/*.cpp`: `check.py` fails naming the file and line; add `// dep: D-24` and it passes.

## On a device (not run by the builder - see the PR)

- `GET /api/settings/effective` on the spare device validates against the schema: `python3 tools/contract-check.py http://192.168.1.128`.
- MQTT alerts on with MQTT off: `POST /api/alerts/test?channel=all` → `/api/alerts/recent` shows `mqtt: skipped "MQTT is off"`.
- Home Assistant: alerts off removes the SQMeter "Alerts" switch entity; alerts on brings it back.
- Heap: `/api/status` `freeHeap` / `stackFree.loop` before and after opening Settings (the report is built on the async task).
