# Budgets: Li-ion battery and fuel gauge

Constitution IV / FR-016. Baselines measured on `feature/travel-allsky` at 3bc6f6a (spec 031
implemented, before any 032 code); after values filled in by T025, T049 and T063.

| Budget | Limit | Before (3bc6f6a) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (PlatformIO "Flash used") | OTA slot 1,835,008 B (SIZE-04) | 1,512,047 B (82.4%) | | |
| Firmware `esp32dev-ble` (PlatformIO "Flash used") | OTA slot 1,835,008 B; 95% line 1,743,257 B | 1,704,271 B (92.9%) | | |
| `firmware.bin` `esp32dev` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,562,032 B (85.1%) | | |
| `firmware.bin` `esp32dev-ble` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,757,136 B (95.8%) | | |
| Static RAM `esp32dev` | | 52,812 B | | |
| Static RAM `esp32dev-ble` | | 63,084 B | | |
| UI gzip JS+CSS | growth <= 10 KB (SIZE-02, `tools/ui/size_check.py`) | 122,631 B (`tools/ui/size-baseline.json`) | | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,615 B | | |
| Main config JSON, worst case (long strings, both sky sensors, display with 7 pages) | 5,100 B | 3,529 B | | |
| Alerts Main part, defaults | 3,900 B | 995 B | | |
| Alerts Client part, defaults | 3,900 B | 285 B | | |
| Readings worst case (`test_worst_case_document_fits`) | pool `Readings::DOC_CAPACITY` 4,608 B with 20% headroom; MQTT payload < 3,072 B | pool 3,424 B, payload 1,760 B | | |
| `/api/status` worst case (`test_status_document_fits`) | pool `Core::STATUS_DOC_CAPACITY` 7,680 B with 20% headroom | 4,830 B | | |

`flash_budget.py` measures `firmware.bin`, which is about 50 KB larger than PlatformIO's "Flash
used" figure that the spec 030/031 budgets quote. By that measure the Bluetooth build is already
at 95.8% of the slot before any 032 code (`tools/firmware/size-baseline.json` still records
main's 1,724,848 B), so CI's SIZE-04 step fails on this branch independent of this feature; the
growth of specs 030-032 needs either trimming or a recorded `--reason` before the PR.

Config sizes were measured by compiling `lib/ConfigModel` with the same long-string set as
`test_display_worst_case_config_fits`.
