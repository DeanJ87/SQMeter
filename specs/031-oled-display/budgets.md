# Budgets: OLED display

Constitution IV / FR-016. Baselines measured on `feature/travel-allsky` at e089b8f (spec 030
implemented, before any 031 code); after values filled in by T023 and T052.

| Budget | Limit | Before (e089b8f) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,498,403 B (81.7%) | | |
| Firmware `esp32dev-ble` (app image) | OTA slot 1,835,008 B; CI fails above 95% (1,743,257 B) | 1,690,667 B (92.1%) | | target <= 9 KB |
| Static RAM `esp32dev` | | 52,796 B | | |
| Static RAM `esp32dev-ble` | | 63,068 B | | |
| UI gzip JS+CSS | growth <= 10 KB (SIZE-01/02, `tools/ui/size_check.py`) | size record 120,001 B (`tools/ui/size-baseline.json`, unchanged since spec 030 T057) | | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,438 B | 2,615 B (T006-T008) | +177 B |
| Main config JSON, worst case (long strings, both sky sensors; + display with 7 pages after) | 5,100 B | 3,295 B | 3,530 B (T006-T008, `test_display_worst_case_config_fits`) | +235 B |
| `/api/status` document, worst case | pool `Core::STATUS_DOC_CAPACITY` 7,680 B with 20% headroom | 4,734 B on the device (`test_status_document_fits`) | | |

Config sizes were measured by compiling `lib/ConfigModel` at e089b8f and at the change with the same
long-string set as `test_sky_worst_case_config_fits` (SSID 32, WiFi password 63, hostname 32, broker
64, MQTT user/password 64, topic 64, discovery prefix 32, OTA password 64, auth user 32 / password 64,
NTP servers 64, timezone 64, device name 32, BLE passkey 6).

The framebuffer (1,024 B) is heap, allocated only when the display is enabled, so it does not show
in static RAM; a device without a display allocates none.
