# Quickstart: validating the OLED display

Commands run from the repo root. Device scenarios use the **spare** device only, with the owner's
permission; nothing here is run against the main device.

## 1. Logic and contracts (no hardware)

```sh
pio test -e native -f test_display_logic -f test_config_model -f test_config_migration \
  -f test_device_core -f test_settings_deps -f test_config_ranges
pio run -e esp32dev && pio run -e esp32dev-ble          # -Werror, sizes into budgets.md
python tools/api/openapi.py --check
python tools/quality/check.py                            # no new findings (DASH, DS, deps, i18n)
cd web && npx vitest run && npx tsc --noEmit && npx eslint src
```

Expected: the display tests cover each page's layout ([contracts/screen.md](contracts/screen.md)),
the `--` marker for stale/faulted values, ellipsis, pixel shift, the controller's timeout/night/
button/wake/blank transitions ([data-model.md](data-model.md)), detection back-off, config defaults,
validation messages ([contracts/config.md](contracts/config.md)) and a pre-031 config loading with the
display off.

## 2. Demo device

`cd web && npm run dev` (demo mode): Settings → Device → Display shows "Detected"; Wake returns OK;
`/api/status` `display.on` turns true, `display.page` follows the configured pages and interval;
Blank turns it off. The API explorer lists `POST /api/display/wake` and `/blank`.

## 3. Device: no display (SC-004)

Flash the spare device without an OLED. `/api/status` → `display.detected: false`; Settings shows the
Display card controls disabled with "No display detected"; the main loop stack, heap and
`/api/sensors` output are unchanged apart from the new status fields (compare with the baseline in
budgets.md).

## 4. Device: display fitted (US1, US2)

Fit an SSD1306 128×64 at 0x3C on the main I2C port. Enable it:

1. Boot with no network: the Sky page appears within 5 s with the authoritative reading (SC-001).
2. Cover/unplug the sky sensor: the value becomes `--` within one read interval, never the old value.
3. Select Sky, Environment, Network with a 5 s cycle: exactly those pages, in order, 5 s each; GPS
   disabled → its page skipped.
4. Network page shows name and IP; in setup-hotspot mode, the AP name and 192.168.4.1 (SC-003).
5. Unplug the OLED while running → `detected: false` within ~15 s; plug back → resumes.

## 5. Device: night (US3, SC-002)

Set a location and a night limit that makes "now" night (e.g. `alerts.nightSunAltitudeDeg` 0 after
sunset): the display is dim, shows the night layout and turns off after the timeout even with
"Never"; with "Stay on at night" it stays on (Settings shows "Uses light at night"). Clear the
location: night mode applies at all hours and "Stay on at night" is disabled with "Location unknown".

## 6. Device: wake (US4)

Display off → Settings Wake → on for the timeout. Configure a button on a free GPIO (AUX): press
wakes; press again shows the next page. A pin used by I2C/GPS/rain/wind is rejected with "Display:
wake button pin is already in use".

## Results (implementation round 2, 2026-10-17)

1. **Logic and contracts**: pass. `pio test -e native` 373/373; `esp32dev` and `esp32dev-ble` build
   with -Werror (sizes in budgets.md); `openapi.py --check` OK (34 operations); `quality/check.py`
   0 findings; web `vitest run` 57 files / 687 tests, `tsc --noEmit` and `eslint src` clean; i18n
   `check.mjs` and `gen_device_catalog.py --check` OK; `dashboard/check.py` OK. `mkdocs build --strict`
   not run (mkdocs not installed here; CI runs it).
2. **Demo device**: pass, driven through the demo core in Node (no browser): settings report
   `displayDetected: true`; Blank → 200 `{"success":true}`, `display.on` false, `page` null; Wake → 200,
   `on` true and `page` stepping sky → environment → safety every 5 s (the demo's pages and interval);
   the API explorer's OpenAPI lists both routes. The Display card's states are covered by Vitest
   (`DisplayCard.test.tsx`, MSW).
3. to 6. **Device**: open for the PR; spare device only, with the owner's permission (no device
   contacted in this round).
