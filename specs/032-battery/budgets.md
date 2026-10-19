# Budgets: Li-ion battery and fuel gauge

Constitution IV / FR-016. Baselines measured on `feature/travel-allsky` at 3bc6f6a (spec 031
implemented, before any 032 code); after values filled in by T025, T049 and T063.

| Budget | Limit | Before (3bc6f6a) | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` (PlatformIO "Flash used") | OTA slot 1,835,008 B (SIZE-04) | 1,512,047 B (82.4%) | 1,527,423 B (83.2%) after US1 (T025); 1,538,739 B (83.9%) after US2-US3 (T049); 1,540,911 B (84.0%) at the end of round 1 (US4); unchanged after round 2 (T063: web, demo and docs only) | +28,864 B |
| Firmware `esp32dev-ble` (PlatformIO "Flash used") | OTA slot 1,835,008 B; 95% line 1,743,257 B | 1,704,271 B (92.9%) | 1,719,643 B (93.7%) after US1 (T025); 1,731,207 B (94.3%) after US2-US3 (T049); 1,733,531 B (94.5%, 9,726 B to the 95% line) at the end of round 1 (US4); unchanged after round 2 (T063) | +29,260 B: well over the 10 KB target; specs 031+032 together +43,260 B over the spec 030 figure (1,690,667 B), leaving 9,726 B under the 95% line |
| `firmware.bin` `esp32dev` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,562,032 B (85.1%) | 1,577,856 B (86.0%) (T025); 1,589,552 B (86.6%) (T049); 1,591,952 B (86.8%) end of round 1; 1,583,280 B (86.3%) after the review fixes (recorded) | +21,248 B |
| `firmware.bin` `esp32dev-ble` (`tools/firmware/flash_budget.py`) | 95% of the slot | 1,757,136 B (95.8%) | 1,772,960 B (96.6%) (T025); 1,784,896 B (97.3%) (T049); 1,787,440 B (97.4%) end of round 1; 1,778,752 B (96.9%) after the review fixes (recorded with a reason) | +21,616 B |
| Static RAM `esp32dev` | | 52,812 B | 53,300 B (T025); 53,356 B (T049); 53,356 B (T063) | +544 B |
| Static RAM `esp32dev-ble` | | 63,084 B | 63,572 B (T025); 63,644 B (T049); 63,644 B (T063) | +560 B |
| UI gzip JS+CSS | growth <= 10 KB (SIZE-02, `tools/ui/size_check.py`) | 122,631 B (`tools/ui/size-baseline.json`) | 127,916 B (T063, recorded in `tools/ui/size-baseline.json`): Battery card and glance tile, System rows, Settings Battery card, Safety and Alerts groups, their strings | +5,285 B (limit 10 KB) |
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
main's 1,724,848 B), so CI's SIZE-04 step failed on this branch independent of this feature; the
growth of specs 030-032 needed either trimming or a recorded `--reason` before the PR (done at the
review fixes, below).

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

Round 2 (T063) changed no firmware: the web UI, the demo core (`tools/demo-core`, rebuilt to
WebAssembly) and the docs. The config, alerts part, readings and status worst cases are the round 1
figures above, still pinned by their native tests (442 passing). Device heap and loop `stackFree`
are to be measured on the spare device for the PR (not done here: no device contacted). The firmware
trimming listed above is still open before the PR.

Review fixes (after round 2): the firmware no longer links libc's scanf engine (`LanguagePack` and
the RG-15 status lines parse with `strtol`), which saves 9,872 B per build; the fixes themselves
(power-saving freshness, alerts at power-on, the wake check, the display resume, the recovered alert)
add about 1.2 KB. `tools/firmware/size-baseline.json` now records both builds (esp32dev 1,583,280 B,
esp32dev-ble 1,778,752 B) with an `overBudgetReason` for the Bluetooth build, so CI's SIZE-04 step
passes with the waiver shown. Justification for the PR: specs 030-032 together put the Bluetooth
build about 54 KB above main (96.9% of the slot, 54.9 KB free). Getting under 95% needs about
35 KB more, and the remaining large items are framework code (WiFi, lwIP, mbedTLS, the Bluetooth
controller) and the string literals of features the specs require. The standard build stays at
86.3%. Whether to accept the waiver or drop features for it is the owner's call at the PR.

## Resolved for the merge (no waiver)

The `overBudgetReason` waiver is removed. Two changes bring every build under 95% of its app
slot (`firmware.bin`, `tools/firmware/flash_budget.py`, recorded in `tools/firmware/size-baseline.json`):

1. **No C++ exceptions** (`CONFIG_COMPILER_CXX_EXCEPTIONS=n`, every env): nothing catches one and an
   allocation failure aborts either way; the unwind tables were about 55 KB per build.
2. **Battery and display drivers only in the ESP32-S3 builds** (`SQM_HAS_BATTERY`,
   `SQM_HAS_DISPLAY`, `include/BoardFeatures.h`, set in `platformio.ini` `[s3]`). The battery is
   travel-board hardware. In the classic `esp32dev` builds the gauge, charger and deep-sleep code
   is left out; no gauge is ever found, so the battery card, rows, rule and alerts behave as on any
   board without a gauge (D-46..D-49). The settings, validation and the hardware-free logic in
   `lib/` are the same in every build, so native tests and the demo cover them.

| Build | `firmware.bin` | Slot | Used |
|---|---|---|---|
| `esp32dev` | 1,513,296 B | 1,835,008 B | 82.5% |
| `esp32dev-ble` | 1,705,392 B | 1,835,008 B | 92.9% |
| `esp32s3-eth` | 1,529,616 B | 6,291,456 B | 24.3% |
| `esp32s3-eth-ble` | 1,718,784 B | 6,291,456 B | 27.3% |
| `esp32s3-zero` | 1,527,600 B | 1,835,008 B | 83.2% |
| `esp32s3-zero-ble` | 1,716,784 B | 1,835,008 B | 93.6% |
| `esp32s3-zero8` | 1,529,616 B | 3,670,016 B | 41.7% |
| `esp32s3-zero8-ble` | 1,718,784 B | 3,670,016 B | 46.8% |

The 4 MB ESP32-S3-Zero Bluetooth build is now the tightest budget (it carries the battery and
display code); the 8 MB Zero-N8R8 has twice the slot (spec 034 open question).
