# Quickstart: validating the battery feature

Commands run from the repo root. Device scenarios use the **spare** device or a travel board only,
with the owner's permission; nothing here is run against the main device. Discharge tests need a
travel board (Rev A) with a matched 1S2P pack.

## 1. Logic and contracts (no hardware)

```sh
pio test -e native -f test_battery_logic -f test_config_model -f test_config_migration \
  -f test_device_core -f test_readings -f test_alpaca_safety -f test_alert_logic \
  -f test_alert_variables -f test_settings_deps -f test_config_ranges -f test_ble_logic
pio run -e esp32dev && pio run -e esp32dev-ble          # -Werror, sizes into budgets.md
python tools/api/openapi.py --check
python tools/quality/check.py                            # no new findings
cd web && npx vitest run && npx tsc --noEmit && npx eslint src
```

Expected: gauge decoding and the estimating window, the charger truth table with debounce, time
estimates and caps, power policy transitions, safety reasons ([contracts/integrations.md](contracts/integrations.md)),
alert crossings with 5 % hysteresis, `{battery*}` variables, `battery` readings/status documents
([contracts/readings.md](contracts/readings.md)), HA entities present/removed, config defaults,
validation messages and a pre-032 config loading unchanged ([contracts/config.md](contracts/config.md)).
ConformU passes in CI.

## 2. Demo device

`cd web && npm run dev`: the dashboard shows a Battery card discharging; the demo panel "USB power"
switch turns it to charging then charged; presets "Battery low" and "Charge fault" fire the alerts and
the glance tile; enabling the battery rule in Settings → Safety turns the verdict unsafe with
`Battery 8% < 10%`.

## 3. Device without a gauge (SC-005)

Spare device, no gauge: `/api/sensors` has only `"battery": {"present": false}`; no Battery card, glance
tile or HA battery entities; Battery settings disabled with "No battery gauge detected"; ConformU and the
contract tests unchanged.

## 4. Travel board (US1, US4)

1. On battery: Battery card shows percent, voltage, Discharging and time to empty.
2. Plug USB-C: Charging within 5 s (SC-002); unplug: Discharging within 5 s.
3. MQTT discovery on: HA shows Battery (battery class), voltage, charging state and time to empty.
4. An alert template with `{battery} {battery_state} {battery_time_left}` renders the values (Test).

## 5. Warnings and safety (US2)

Set the low level above the current charge: one Battery low alert, no repeat until charge rises 5 %
above (SC-003 over a full discharge). Enable the battery rule with a limit above the charge: verdict
unsafe `Battery N% < M%`, a Battery rule alert and (BLE build, paired phone) a phone alarm. Unplug the
gauge with the rule on: `Battery data missing`.

## 6. Power saving and sleep (US3, SC-004)

Set critical above the charge: Critical alert, `powerSaving: true`, sensor reads every 30 s, display off.
Set shutdown above the charge: Going-to-sleep alert (and phone alarm), unsafe for ≥ 30 s, then deep sleep
(device stops answering). Plug USB-C: the device wakes, joins WiFi and sends Power restored. Full
discharge test: imaging software sees unsafe before the cell protection cuts power, and the reported
charge stays within 5 % of a reference (coulomb counter or a reference gauge) after settling (SC-001).

## Results

Round 2 (no hardware, no device contacted):

- **1. Logic and contracts**: `pio test -e native` 442 cases passed; `pio run -e esp32dev` and
  `-e esp32dev-ble` build with `-Werror` (84.0% / 94.5% flash); `tools/api/openapi.py --check` OK;
  `tools/quality/check.py` no new findings; web `vitest run` (745), `tsc --noEmit` and `eslint src`
  pass; i18n, settings-deps and dashboard checks pass. ConformU: round 1 (Docker, both rule modes).
- **2. Demo device**: checked by `web/src/demo/__tests__/battery.test.ts` against the rebuilt
  WebAssembly core: the pack discharging, USB power to charging, a charging fault, power saving below
  the critical level and the battery rule's `Battery N% < 10%`; the panel's presets set the levels.
  The demo's alerts after the presets are left to a manual look (`npm run dev`).
- **3-6**: open for the PR, on the spare device or a travel board with the owner's permission.
