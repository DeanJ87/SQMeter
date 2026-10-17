# Budgets: OLED display

Constitution IV / FR-016. Baselines measured on `feature/travel-allsky` at e089b8f (spec 030
implemented, before any 031 code); after values filled in by T023 and T052.

| Budget | Limit | Before (e089b8f) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,498,403 B (81.7%) | 1,512,199 B (82.4%) after US1-US4 firmware (T023); 1,512,047 B (82.4%) at the end of implementation round 1 | +13,644 B |
| Firmware `esp32dev-ble` (app image) | OTA slot 1,835,008 B; CI fails above 95% (1,743,257 B) | 1,690,667 B (92.1%) | 1,704,279 B (92.9%) after US1-US4 firmware (T023); 1,704,271 B (92.9%, 38,986 B to the fail line) at the end of round 1 | +13,604 B: over the 9 KB target |
| Static RAM `esp32dev` | | 52,796 B | 52,812 B (T023) | +16 B |
| Static RAM `esp32dev-ble` | | 63,068 B | 63,084 B (T023) | +16 B |
| UI gzip JS+CSS | growth <= 10 KB (SIZE-01/02, `tools/ui/size_check.py`) | size record 120,001 B (`tools/ui/size-baseline.json`, unchanged since spec 030 T057) | 118.2 KB vite JS+CSS, +1.0 KB over the record (round 1: types, schema, settings dependencies; the Display card is not built yet) | +1.0 KB |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,438 B | 2,615 B (T006-T008) | +177 B |
| Main config JSON, worst case (long strings, both sky sensors; + display with 7 pages after) | 5,100 B | 3,295 B | 3,530 B (T006-T008, `test_display_worst_case_config_fits`) | +235 B |
| `/api/settings/effective` report (`test_report_document`) | < 5 KB in spec 020's plan; now < 6 KB | under 5,000 B | 5,256 B with the eight display entries (T011) | about +0.3 KB; transient, per request; test bound raised to 6,000 B |
| `/api/status` document, worst case | pool `Core::STATUS_DOC_CAPACITY` 7,680 B with 20% headroom | 4,734 B on the device (`test_status_document_fits`) | 4,830 B with `display` (T019) | +96 B, capacity unchanged |

Config sizes were measured by compiling `lib/ConfigModel` at e089b8f and at the change with the same
long-string set as `test_sky_worst_case_config_fits` (SSID 32, WiFi password 63, hostname 32, broker
64, MQTT user/password 64, topic 64, discovery prefix 32, OTA password 64, auth user 32 / password 64,
NTP servers 64, timezone 64, device name 32, BLE passkey 6).

Firmware growth (T023) is above the plan's 6-9 KB: the display symbols themselves (renderer
1.5 KB, fonts 0.5 KB, controller, link, transport, glue, view model, status, routes) are about
10.6 KB, and the settings (JSON in/out, validation, the eight settings-dependency entries) and the
device messages make up the rest. The firmware grows the same whether or not a display is fitted;
the controller, renderer and transport are built in, the framebuffer is not. Spec 032 shares the
remaining 38,978 B on the Bluetooth build before CI's 95% line.

The framebuffer (1,024 B) is heap, allocated only when the display is enabled, so it does not show
in static RAM; a device without a display allocates none.

Device heap and loop `stackFree` with and without a display: not measured (no device contacted in
implementation round 1); to be measured on the spare device only, with the owner's permission, and
noted for the PR.
