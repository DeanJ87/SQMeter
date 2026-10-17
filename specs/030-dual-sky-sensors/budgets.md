# Budgets: Two sky sensors (SQM and SQM-L)

Constitution IV / FR-015. Baselines measured on `feature/v0.4-alpha` at 47bf118 (before any 030
code), after values filled in by T020 and T057.

| Budget | Limit | Before (47bf118) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,487,779 B (81.1%) | | |
| Firmware `esp32dev-ble` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,682,263 B (91.7%) | | |
| Static RAM `esp32dev` | | 52,748 B | | |
| Static RAM `esp32dev-ble` | | 63,060 B | | |
| UI gzip JS+CSS (`vite build`) | growth <= 10 KB (SIZE-02) | 117.86 KB (CSS 8.04 + JS 30.44 + 79.38) | | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,139 B | | |
| Main config JSON, long strings in every text field | 5,100 B | 2,874 B | | |
| Readings document, worst case (all groups ok) | MQTT buffer 3,072 B | 1,247 B serialised, 2,336 B pool (research) | | |

Long strings: SSID 32, WiFi password 63, hostname 32, broker 64, MQTT user/password 64, topic 64,
discovery prefix 32, OTA password 64, auth user 32 / password 64, NTP servers 64, timezone 64,
device name 32, BLE passkey 6 (the same set the native test `test_sky_worst_case_config_fits` uses).

Device heap and loop `stackFree` with one and two sensors: measured on the spare device only
(T057), noted in the PR.
