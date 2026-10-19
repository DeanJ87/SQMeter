# Budgets: Settings backup and restore

Constitution IV. "Before" is the branch this merges into (specs 030-032 and 034 merged, C++
exceptions off, OLED and battery drivers only in the S3 builds); "after" is with spec 033.
Firmware is `firmware.bin` against each build's own app slot (`tools/firmware/flash_budget.py`,
SIZE-04, fails above 95%), recorded in `tools/firmware/size-baseline.json`. No waiver.

| Budget | Limit | Before | After | Delta |
|---|---|---|---|---|
| Firmware `esp32dev` | 95% of 1,835,008 B (1,743,257 B) | 1,513,296 B (82.5%) | 1,536,560 B (83.7%) | +23,264 B |
| Firmware `esp32dev-ble` | 95% of 1,835,008 B | 1,705,392 B (92.9%) | 1,729,424 B (94.2%) | +24,032 B |
| Firmware `esp32s3-eth` | 95% of 6,291,456 B | 1,529,616 B (24.3%) | 1,552,144 B (24.7%) | +22,528 B |
| Firmware `esp32s3-eth-ble` | 95% of 6,291,456 B | 1,718,784 B (27.3%) | 1,742,448 B (27.7%) | +23,664 B |
| Firmware `esp32s3-zero` | 95% of 1,835,008 B | 1,527,600 B (83.2%) | 1,550,144 B (84.5%) | +22,544 B |
| Firmware `esp32s3-zero-ble` | 95% of 1,835,008 B | 1,716,784 B (93.6%) | 1,740,432 B (94.8%, 2,825 B under the line) | +23,648 B |
| Firmware `esp32s3-zero8` | 95% of 3,670,016 B | 1,529,616 B (41.7%) | 1,552,160 B (42.3%) | +22,544 B |
| Firmware `esp32s3-zero8-ble` | 95% of 3,670,016 B | 1,718,784 B (46.8%) | 1,742,448 B (47.5%) | +23,664 B |
| UI gzip JS + CSS (`tools/ui/size_check.py`) | growth <= 10 KB per change (SIZE-01) | 127,916 B | 132,879 B | +4,963 B |
| LittleFS fill (UI + a 64 KB language file) | 75% of 448 KB (SIZE-02) | | 67% | |
| Restore request body | 20,480 B on the device (413 above, before parsing); 64 KB in the web UI | | | |
| Heap during a preview | measured on a device (T037) | | typical 25-30 KB, worst case about 60 KB, transient (research R7) | |

The 033 code itself is unchanged by the merge: about 24 KB per build, after moving the preview
wording to the web UI (research R7, R10). The tightest build is now the 4 MB ESP32-S3-Zero with
Bluetooth, which carries the battery and display code too; the 8 MB Zero-N8R8 has twice the slot
(spec 034 open question).

Heap and stack during a backup and a preview, and the derivation time, are still to be measured on
the spare device (T037).
