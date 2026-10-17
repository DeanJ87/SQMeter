# Budgets: Li-ion battery and fuel gauge

Constitution IV / FR-016. Baselines measured on `feature/travel-allsky` at 3bc6f6a (spec 031
implemented, before any 032 code); after values filled in by T025, T049 and T063.

| Budget | Limit | Before (3bc6f6a) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (PlatformIO "Flash used") | OTA slot 1,835,008 B (SIZE-04) | 1,512,047 B (82.4%) | 1,527,423 B (83.2%) after US1 (T025); 1,538,739 B (83.9%) after US2-US3 (T049); 1,540,911 B (84.0%) at the end of round 1 (US4) | +28,864 B |
| Firmware `esp32dev-ble` (PlatformIO "Flash used") | OTA slot 1,835,008 B; 95% line 1,743,257 B | 1,704,271 B (92.9%) | 1,719,643 B (93.7%) after US1 (T025); 1,731,207 B (94.3%) after US2-US3 (T049); 1,733,531 B (94.5%, 9,726 B to the 95% line) at the end of round 1 (US4) | +29,260 B: well over the 10 KB target |
| `firmware.bin` `esp32dev` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,562,032 B (85.1%) | 1,577,856 B (86.0%) (T025); 1,589,552 B (86.6%) (T049); 1,591,952 B (86.8%) end of round 1 | +29,920 B |
| `firmware.bin` `esp32dev-ble` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,757,136 B (95.8%) | 1,772,960 B (96.6%) (T025); 1,784,896 B (97.3%) (T049); 1,787,440 B (97.4%) end of round 1 | +30,304 B |
| Static RAM `esp32dev` | | 52,812 B | 53,300 B (T025); 53,356 B (T049) | +544 B |
| Static RAM `esp32dev-ble` | | 63,084 B | 63,572 B (T025); 63,644 B (T049) | +560 B |
| UI gzip JS+CSS | growth <= 10 KB (SIZE-02, `tools/ui/size_check.py`) | 122,631 B (`tools/ui/size-baseline.json`) | | |
| Main config JSON, defaults (`toJson(false, false)`) | 5,100 B | 2,615 B | 2,952 B (T006-T008) | +337 B |
| Main config JSON, worst case (long strings, both sky sensors, display with 7 pages) | 5,100 B | 3,529 B | 3,869 B with the battery section, every pin set (`test_battery_worst_cases_fit`) | +340 B |
| Alerts Main part, defaults | 3,900 B | 995 B | 995 B, byte for byte (`test_battery_alerts_part`) | 0 |
| Alerts Client part, defaults | 3,900 B | 285 B | 285 B, byte for byte | 0 |
| Readings worst case (`test_worst_case_document_fits`) | pool `Readings::DOC_CAPACITY` 4,608 B with 20% headroom; MQTT payload < 3,072 B | pool 3,424 B, payload 1,760 B | pool 3,872 B, payload 2,020 B with the full battery object; `DOC_CAPACITY` raised to 5,120 B (T020) | +448 B pool (+512 B capacity, transient heap) |
| Alerts Battery part (`alertsBatt`), defaults / every template at its limit | 3,900 B | none | 396 B / 2,376 B (`test_battery_worst_cases_fit`) | new key |
| `/api/settings/effective` report (`test_report_document`) | test bound 6,000 B after spec 031 | 5,256 B | 6,850 B with the eighteen battery entries; bound raised to 7,500 B | +1.6 KB, transient per request |
| `/api/status` worst case (`test_status_document_fits`) | pool `Core::STATUS_DOC_CAPACITY` 7,680 B with 20% headroom | 4,830 B | 5,365 B with `battery`, `sensors.battery` and `diagnostics.battery` (T021) | +535 B, capacity unchanged |

`flash_budget.py` measures `firmware.bin`, which is about 50 KB larger than PlatformIO's "Flash
used" figure that the spec 030/031 budgets quote. By that measure the Bluetooth build is already
at 95.8% of the slot before any 032 code (`tools/firmware/size-baseline.json` still records
main's 1,724,848 B), so CI's SIZE-04 step fails on this branch independent of this feature; the
growth of specs 030-032 needs either trimming or a recorded `--reason` before the PR.

Config sizes were measured by compiling `lib/ConfigModel` with the same long-string set as
`test_display_worst_case_config_fits`.

Firmware growth after US1 (T025) is about 15 KB per build, above the 10 KB target, before the
safety rule, alerts, power policy and integrations land. The largest share is `lib/ConfigModel`
(49,039 B to 55,390 B: the battery section's JSON in and out, its validation and messages, the
six alert events and the third stored part), then the gauge driver, `lib/BatteryLogic`, the
readings/status writers and the eighteen settings-dependency entries. Trimming (for example
table-driven range checks in `ConfigValidate.cpp`) is open for round 2; the Bluetooth build is
at 93.7% by PlatformIO's measure and 96.6% by `firmware.bin`.

Static RAM grew 488 B: the battery snapshot (reading, estimates) inside the static sensor
snapshot copies and the driver's decoders.

After US2-US3 (T049) the growth is about 27 KB per build: the safety rule, seven alert types and
their wording, the power policy and manager (deep sleep, RTC wake), the dispatcher's idle check and
`libm`'s `exp`/`floor` for the rate average and the reasons. This is well over the plan's 10 KB
and leaves the Bluetooth build 12,050 B under CI's 95% line by PlatformIO's measure, before the
Home Assistant entities, template variables and BLE summary fields (US4) land. Trimming before
the PR is required (candidates: table-driven range checks and messages in `ConfigValidate.cpp`,
shared formatting in the alert wording, the rate average without `exp`).
