# Tasks: ESP32-S3 boards

**Input**: [spec.md](spec.md), [plan.md](plan.md)

## Phase 1: Build targets (US1) - done in d84f80a

- [X] T001 [US1] `esp32s3-eth` and `esp32s3-eth-ble` envs (16 MB, octal PSRAM, native USB CDC) in platformio.ini
- [X] T002 [US1] `esp32s3-zero` and `esp32s3-zero-ble` envs (4 MB, quad PSRAM, `partitions.csv`) in platformio.ini
- [X] T003 [US1] `esp32s3-zero8` and `esp32s3-zero8-ble` envs (8 MB, octal PSRAM) in platformio.ini
- [X] T004 [US1] `partitions_16MB.csv` (two 6 MB app slots) and `partitions_8MB.csv` (two 3.5 MB app slots), nvs/otadata at the 4 MB layout's offsets (FR-002)
- [X] T005 [US1] Per-env app slot in tools/firmware/flash_budget.py (read from `board_build.partitions`), tests in tools/firmware/test_firmware_tools.py (FR-004)
- [X] T006 [US1] All eight builds recorded in tools/firmware/size-baseline.json
- [X] T007 [US1] `build-s3` matrix job in .github/workflows/build.yml (FR-005)

## Phase 2: Board pin defaults (US2) - done in d84f80a

- [X] T008 [US2] test/test_board_profiles: every profile's defaults are usable GPIOs, S3-ETH avoids the W5500 pins, ADC1 ranges
- [X] T009 [US2] lib/ConfigModel/include/BoardProfile.h: classic, S3-ETH and S3-Zero profiles, chosen by `-DSQM_BOARD_*` (FR-003)
- [X] T010 [US2] Config defaults and validation from the build's profile in lib/ConfigModel/src/ConfigModel.cpp and ConfigValidate.cpp; vane message carries `{adc1Pins}` (strings, translations, demo core)

## Phase 3: Install and update (US3) - open

- [X] T011 [US3] Per-board release files `sqmeter-<board>-firmware-`, `-ble-firmware-`, `-littlefs-` (lib/ReleaseLogic `prefixesFor`, include/BuildTarget.h) and the update check picking its own board's files; the image marker carries the board's layout (`l2`, `s8`, `s16`) so OTA refuses another layout's image (FR-006)
- [X] T012 [US3] Release job builds and attaches every S3 env's firmware, LittleFS and USB packages (`usb_package.py --chip esp32s3`) (FR-007)
- [ ] T012a [US3] Browser flasher manifest per S3 board on sqmeter.dev (FR-007); until then S3 boards are installed with esptool from the USB package
- [ ] T013 [US3] Settings pin hints from the running board's profile (FR-008); mockup reviewed before build

## Phase 4: Board hardware (US4) - open

- [ ] T014 [US4] W5500 Ethernet on the S3-ETH (FR-009)
- [ ] T015 [US4] WS2812 status LED on GPIO 21 (FR-010)
- [ ] T016 [US4] Travel profile rain/wind default pins that don't clash with the fuel gauge and charger lines (FR-011)

## Phase 5: Validation - open

- [ ] T017 Hardware validation on a real S3-ETH, S3-Zero and S3-Zero-N8R8: boot, sensors on both I2C buses, WiFi, OTA, BLE build (FR-012, SC-004)

## Phase 6: Board features (specs 031, 032)

- [X] T018 OLED display and battery drivers built only for the S3 boards (`SQM_HAS_DISPLAY`, `SQM_HAS_BATTERY` in `[s3]`, include/BoardFeatures.h); classic builds report them as not detected
