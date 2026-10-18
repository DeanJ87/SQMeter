# Implementation Plan: ESP32-S3 boards

**Branch**: `feature/v0.4-alpha` | **Date**: 2026-10-19 | **Spec**: [spec.md](spec.md)

## Summary

Add build targets for the Waveshare ESP32-S3-ETH (main rig) and ESP32-S3-Zero / Zero-N8R8 (travel
rig), each with and without Bluetooth. Board differences live in one compile-time board profile in
`lib/ConfigModel` (default pins, usable GPIOs, ADC1 range), so config defaults and validation follow
the build's board and stay testable natively. The flash budget checks each env against its own
partition table. Done in d84f80a; packaging, UI hints and board hardware (Ethernet, LED) remain.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5), Python 3.12 tools

**Primary Dependencies**: PlatformIO, pioarduino platform; boards `esp32-s3-devkitc1-n16r8`,
`esp32-s3-fh4r2`, `esp32-s3-devkitc1-n8r8`

**Storage**: NVS (offsets unchanged across layouts), LittleFS

**Testing**: `pio test -e native` (`test_board_profiles`), `tools/firmware/test_firmware_tools.py`

**Target Platform**: ESP32 (classic) and ESP32-S3

**Constraints**: each image at or below 95% of its app slot (SIZE-04); nvs/otadata offsets fixed

## Constitution Check

| Principle | How | Result |
|---|---|---|
| Hardware-free logic in `lib/` | Board profile is `constexpr` in `lib/ConfigModel`, selected by `-DSQM_BOARD_*` | Pass |
| Tests first | `test_board_profiles` covers every profile's defaults and masks | Pass |
| Size budgets (IV) | Per-env app slot in `flash_budget.py`; all eight builds in `size-baseline.json` | Pass |
| Quiet UI (V) | No UI change in the built part; pin hints (FR-008) still to design | n/a |

## Project Structure

```text
platformio.ini                          # [s3] shared flags, six S3 envs
partitions_16MB.csv, partitions_8MB.csv # S3-ETH and S3-Zero-N8R8 layouts
lib/ConfigModel/include/BoardProfile.h  # per-board pins, GPIO mask, ADC1 range
lib/ConfigModel/src/ConfigModel.cpp     # defaults from the profile
lib/ConfigModel/src/ConfigValidate.cpp  # validation against the profile
test/test_board_profiles/               # native tests
tools/firmware/flash_budget.py          # per-env slot from board_build.partitions
tools/firmware/size-baseline.json       # all eight builds
.github/workflows/build.yml             # build-s3 matrix job
```

## Remaining work

1. Release and OTA asset naming per board, and the update check choosing its own board's asset.
2. Release job and USB web flasher: build and attach every S3 env (bootloader, partitions, app,
   LittleFS) and a per-board flasher manifest.
3. Settings pin hints from the running board's profile (needs a mockup reviewed before build).
4. W5500 Ethernet on the S3-ETH.
5. WS2812 status LED.
6. Travel profile rain/wind default pins (currently off: those pins carry the fuel gauge and
   charger on the travel board).
7. Hardware validation on each real board.
