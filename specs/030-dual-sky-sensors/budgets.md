# Budgets: Two sky sensors (SQM and SQM-L)

Constitution IV / FR-015. Baselines measured on `feature/v0.4-alpha` at 47bf118 (before any 030
code), after values filled in by T020 and T057.

| Budget | Limit | Before (47bf118) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,487,779 B (81.1%) | 1,493,855 B (81.4%) after US1 (T020); 1,497,915 B (81.6%) after US2-US4 firmware; 1,498,403 B (81.7%) after the review fixes (T057) | +10,624 B |
| Firmware `esp32dev-ble` (app image) | OTA slot 1,835,008 B (SIZE-04) | 1,682,263 B (91.7%) | 1,686,199 B (91.9%) after US1 (T020); 1,690,187 B (92.1%) after US2-US4 firmware; 1,690,667 B (92.1%, 144,341 B free) after the review fixes (T057) | +8,404 B |
| Static RAM `esp32dev` | | 52,748 B | 52,796 B (T020, unchanged at T057) | +48 B |
| Static RAM `esp32dev-ble` | | 63,060 B | 63,068 B (T020, unchanged at T057) | +8 B |
| UI gzip JS+CSS (`vite build`) | growth <= 10 KB (SIZE-01/02, `tools/ui/size_check.py`) | 117.86 kB (CSS 8.04 + JS 30.44 + 79.38); size record 116,875 B | 121.26 kB (CSS 8.04 + JS 31.17 + 82.05); size record 120,001 B (T057) | +3.40 kB vite, +3,126 B record: passes |
| LittleFS fill (UI + a 64 KB language file) | 75% of 448 KB (SIZE-02) | | 288 KB, 64% (T057) | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,139 B | | |
| Main config JSON, long strings in every text field | 5,100 B | 2,874 B | below the limit with both sensors configured (test_sky_worst_case_config_fits) | |
| Readings document, worst case (all groups ok) | MQTT buffer 3,072 B; pool `Readings::DOC_CAPACITY` 4,608 B with 20% headroom | 1,247 B serialised, 2,336 B pool (research) | 3,424 B pool with both sky sensors (test_worst_case_document_fits) | +1,088 B pool |
| `/api/status` document, worst case (every sensor, both sky sensors, longest strings) | pool `Core::STATUS_DOC_CAPACITY` 7,680 B (was 7,168 B) with 20% headroom | | 4,734 B on the device (16-byte slots; host 9,102 B), test_status_document_fits (T057) | +512 B capacity |

Long strings: SSID 32, WiFi password 63, hostname 32, broker 64, MQTT user/password 64, topic 64,
discovery prefix 32, OTA password 64, auth user 32 / password 64, NTP servers 64, timezone 64,
device name 32, BLE passkey 6 (the same set the native test `test_sky_worst_case_config_fits` uses).

The sample buffers (3 KB per sensor) are heap, allocated in the first successful `begin()`, so
they do not show in static RAM; a device with the SQM-L off allocates none for it.

Device heap and loop `stackFree` with one and two sensors: measured on the spare device only
(T057), noted in the PR. Not measured in the review-fix round (no device contacted): still open.

`/api/status` was not pinned before; `test_status_document_fits` (test/test_device_core) writes the
library's sections for real and models the device's own sections (firmware, memory, partitions,
time, WiFi, MQTT, connections) member for member with their longest strings, and converts the
host's slot size to the ESP32's. `createStatusJson` now logs a warning if the pool ever overflows.

Firmware growth is above the plan's "< 6 KB" expectation (+10.6 KB classic / +8.4 KB BLE: mostly
the non-blocking driver, the second bus, per-sensor documents, Home Assistant entities and
role-literal reasons; the review fixes added about 0.5 KB, the D-39 gate and named reasons). Both
builds fit the OTA slot; the BLE build is at 92.1% with about 141 KiB left, the tightest budget to
watch in the 3.x port.
