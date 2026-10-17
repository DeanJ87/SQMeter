# Budgets: Two sky sensors (SQM and SQM-L)

Constitution IV / FR-015. Baselines measured on `feature/v0.4-alpha` at 47bf118 (before any 030
code), after values filled in by T020 and T057.

| Budget | Limit | Before (47bf118) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,487,779 B (81.1%) | 1,493,855 B (81.4%) after US1 (T020); 1,497,915 B (81.6%) after US2-US4 firmware | +10,136 B |
| Firmware `esp32dev-ble` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,682,263 B (91.7%) | 1,686,199 B (91.9%) after US1 (T020); 1,690,187 B (92.1%) after US2-US4 firmware | +7,924 B |
| Static RAM `esp32dev` | | 52,748 B | 52,796 B (T020) | +48 B |
| Static RAM `esp32dev-ble` | | 63,060 B | 63,068 B (T020) | +8 B |
| UI gzip JS+CSS (`vite build`) | growth <= 10 KB (SIZE-02) | 117.86 KB (CSS 8.04 + JS 30.44 + 79.38) | | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,139 B | | |
| Main config JSON, long strings in every text field | 5,100 B | 2,874 B | below the limit with both sensors configured (test_sky_worst_case_config_fits) | |
| Readings document, worst case (all groups ok) | MQTT buffer 3,072 B; pool `Readings::DOC_CAPACITY` 4,608 B with 20% headroom | 1,247 B serialised, 2,336 B pool (research) | 3,424 B pool with both sky sensors (test_worst_case_document_fits) | +1,088 B pool |

Long strings: SSID 32, WiFi password 63, hostname 32, broker 64, MQTT user/password 64, topic 64,
discovery prefix 32, OTA password 64, auth user 32 / password 64, NTP servers 64, timezone 64,
device name 32, BLE passkey 6 (the same set the native test `test_sky_worst_case_config_fits` uses).

The sample buffers (3 KB per sensor) are heap, allocated in the first successful `begin()`, so
they do not show in static RAM; a device with the SQM-L off allocates none for it.

Device heap and loop `stackFree` with one and two sensors: measured on the spare device only
(T057), noted in the PR.

Interim note (implementation round 1): the firmware is above the plan's "< 6 KB" expectation
(+10.1 KB / +7.9 KB, mostly the non-blocking driver, the second bus, per-sensor documents, Home
Assistant entities and role-literal reasons) but well inside the OTA slot. The UI gzip "after"
waits for the dashboard and Settings work (T022-T025, T035, T036, T042), measured in T057.
