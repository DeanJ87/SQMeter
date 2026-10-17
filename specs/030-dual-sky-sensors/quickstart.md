# Quickstart: validating two sky sensors

Shapes are in [contracts/](contracts/); decisions in [research.md](research.md).

## 1. Desktop checks (no hardware)

```sh
pio test -e native                       # SkyRole, SkyReadCycle, config, readings, safety, Alpaca, alerts, deps
pio run -e esp32dev && pio run -e esp32dev-ble   # -Werror, size within SIZE-04
python tools/quality/check.py            # no new findings (incl. DASH, DS, settings-deps)
python tools/api/openapi.py && python tools/api/openapi.py --check
cd web && npx vitest run && npx tsc --noEmit && npx eslint src
python3 tools/i18n/gen_device_catalog.py --check && node tools/i18n/check.mjs
tools/demo-core/build.sh                 # SOURCE_HASH current
python3 -m mkdocs build --strict         # where mkdocs is installed
```

Expected native coverage, by scenario:

| Scenario | Proof |
|---|---|
| One-sensor config emits exactly as before (SC-001) | readings/HA/Alpaca/BLE golden tests unchanged; only `sky.source: "sqm"` added |
| Old saved config loads, defaults applied (FR-014) | `test_config_migration` with a pre-030 fixture |
| Authoritative SQM-L fills every single-value field (US2 AC2) | `test_device_core`: derive + readings + safety inputs + Alpaca snapshot + alert vars |
| Authoritative faulted, other ok -> no fallback, unsafe, named (US2 AC3, FR-010) | `test_device_core`, `test_alpaca_safety` |
| Non-authoritative fault never changes the verdict | `test_alpaca_safety` |
| Dark calibration only touches its sensor (US3 AC2) | route logic test of the config update helper |
| HA entities added/removed with enable flags (US4 AC1) | `test_readings` discovery cases |
| Unplug -> fault within one interval, replug -> recover (edge case) | `test_sky_logic` `SkyReadCycle` |
| Worst-case config and readings sizes fit (FR-015) | `test_config_model`, `test_readings` |

## 2. Demo (browser)

`cd web && npm run build:demo && npm run preview:demo`: the demo device has SQM-L enabled with a reading ~0.25 darker.

1. Dashboard: Sky quality shows the SQM large with an "SQM" pill, an SQM-L tile and a Difference
   tile; at-a-glance has a tile per sky sensor.
2. Settings → Sensors: set Authoritative reading to SQM-L, save. The large value, `sky.source`
   (`/api/sensors` in the API view), Alpaca `skyquality` and the BLE summary in the demo panel all
   switch to SQM-L.
3. Demo panel: fail the SQM-L sensor. Safety goes unsafe with `Sensor fault: SQM-L sensor`; the Sky
   card shows the missing sensor and "Use SQM"; `sky` has `status` only; `skySensors.sqm` still
   reports.
4. Disable SQM-L: its card, tile, `skySensors.sqml` and HA entities (demo MQTT view) disappear.

## 3. Device (spare device only, never the main one)

Prerequisites: an ESP32-S3 board with I2C-2 wired (or a classic ESP32 with two free GPIOs set as
I2C-2 pins), two TSL2591s.

1. Update the spare device with the new firmware and UI; confirm `/api/config` loads with the old
   values and `/api/sensors` is unchanged apart from `sky.source`.
2. Fit the second sensor, set the I2C-2 pins, enable SQM-L, restart. `/api/status` `skySensors.sqml.
   detected` is true; both readings update.
3. Cover one sensor: only its `skySensors` entry changes (SC-002).
4. Unplug SQM-L while running: `skySensors.sqml.status` becomes `error` within one read interval;
   replug: back to `ok` without a restart.
5. Dark-calibrate SQM-L with `?sensor=sqml`: only `skyCalibrationL` changes.
6. Record firmware size (both builds), `/api/status` heap and `stackFree` before/after in the PR
   (constitution IV). Run ConformU against the simulator (CI) with zero errors.
