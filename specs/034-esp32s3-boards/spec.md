# Feature Specification: ESP32-S3 boards

**Feature Branch**: `feature/v0.4-alpha`

**Created**: 2026-10-19

**Status**: Partly implemented (build targets, board pin defaults, per-board flash budget, CI in
commit d84f80a). Packaging, board-aware UI hints, Ethernet, status LED and hardware validation open.

## Context

SQMeter has built for one board, the classic ESP32 DevKit (`esp32dev`, `esp32dev-ble`). Two
ESP32-S3 boards are planned:

- **Main rig**: Waveshare ESP32-S3-ETH (ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM, W5500 Ethernet).
- **Travel rig**: Waveshare ESP32-S3-Zero (ESP32-S3FH4R2, 4 MB flash, 2 MB quad PSRAM), or its
  8 MB variant ESP32-S3-Zero-N8R8 (ESP32-S3R8, 8 MB flash, 8 MB octal PSRAM).

The S3 has different usable GPIOs (no 19/20, which are USB; 26-37 are flash and PSRAM on the
octal parts), a different ADC1 range (1-10) and native USB. The classic board's default pins are
not valid on it, so defaults and pin validation have to follow the board the firmware is built for.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build the firmware for my board (Priority: P1)

A builder with an ESP32-S3-ETH or ESP32-S3-Zero builds SQMeter for that board, with or without
Bluetooth, and gets an image that fits the board's own app slot.

**Why this priority**: nothing else works without a build for the board.

**Independent Test**: `pio run -e <env>` for each S3 env; `tools/firmware/flash_budget.py`
checks each against its own partition table.

**Acceptance Scenarios**:

1. **Given** the repo, **When** any of `esp32s3-eth(-ble)`, `esp32s3-zero(-ble)`,
   `esp32s3-zero8(-ble)` is built, **Then** it links and is at or below 95% of its app slot.
2. **Given** a pull request, **When** CI runs, **Then** every S3 env is built and budget-checked.

---

### User Story 2 - Sensible default pins for my board (Priority: P1)

On a freshly flashed S3 board the default I2C, I2C-2, GPS, RG-15, wind and AUX pins are pins that
exist and are free on that board, and Settings rejects a pin the board can't use.

**Why this priority**: classic ESP32 defaults (e.g. SDA 21, SCL 22) are wrong or dangerous on an S3.

**Independent Test**: `pio test -e native -f test_board_profiles`.

**Acceptance Scenarios**:

1. **Given** an S3-ETH build, **When** settings are reset, **Then** the defaults are the S3-ETH
   profile's pins and none is a W5500 pin (9-14).
2. **Given** any S3 build, **When** a pin outside the board's usable GPIOs is saved, **Then** it is
   rejected, and a vane pin outside ADC1 is rejected with the board's ADC1 range in the message.
3. **Given** a classic `esp32dev` build, **Then** defaults and validation are unchanged.

---

### User Story 3 - Install and update an S3 board like the classic one (Priority: P2)

An S3 owner flashes the first image from the web flasher or a release asset, and later updates over
the air with the asset for that board, never another board's.

**Independent Test**: a release built from a tag carries one clearly named asset set per board;
the device's update check picks the asset for its own board.

**Acceptance Scenarios**:

1. **Given** a release, **When** an S3-Zero checks for updates, **Then** it offers the S3-Zero
   image, not the classic or S3-ETH one.
2. **Given** the web flasher, **When** an S3 board is chosen, **Then** it writes that board's
   bootloader, partition table, app and LittleFS images.

---

### User Story 4 - Use the board's own hardware (Priority: P3)

The S3-ETH can use its wired Ethernet; both boards show status on their WS2812 LED; the travel rig
has default pins for rain and wind if they are fitted.

### Edge Cases

- A board whose settings were saved by another board's firmware (pins invalid on this board):
  validation reports them; the device does not drive an invalid pin.
- The 4 MB S3-Zero has the same app slot as the classic board, so it is the tightest S3 budget.
- Changing partition layout needs a USB flash; OTA can't rewrite the partition table.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The build MUST provide `esp32s3-eth`, `esp32s3-eth-ble`, `esp32s3-zero`,
  `esp32s3-zero-ble`, `esp32s3-zero8` and `esp32s3-zero8-ble` envs.
- **FR-002**: The 16 MB and 8 MB boards MUST use their own partition tables
  (`partitions_16MB.csv`, `partitions_8MB.csv`) that keep the nvs and otadata offsets of the 4 MB
  layout, so a USB flash that skips 0x9000-0xDFFF keeps settings.
- **FR-003**: Default pins, usable GPIOs and the ADC1 range MUST come from a per-board profile
  chosen at compile time; the classic board's behaviour MUST NOT change.
- **FR-004**: Each build MUST be checked against its own app slot (SIZE-04) and recorded in
  `tools/firmware/size-baseline.json`.
- **FR-005**: CI MUST build every S3 env on each code change.
- **FR-006**: Release and OTA assets MUST be named per board so a device only offers its own image.
- **FR-007**: The release and the web flasher MUST carry every board's images.
- **FR-008**: Pin hints in Settings MUST show the running board's usable pins and defaults.
- **FR-009**: The S3-ETH SHOULD support its W5500 Ethernet.
- **FR-010**: The boards' WS2812 status LED SHOULD show device state.
- **FR-011**: The travel profile SHOULD carry default pins for rain and wind when they are fitted.
- **FR-012**: Each board MUST be validated on real hardware before its images are offered as
  stable.

### Key Entities

- **Board profile**: per board, default pins (I2C, I2C-2, GPS, RG-15, wind, AUX, status LED,
  battery alert and charger lines), the usable GPIO mask and the ADC1 range.

## Success Criteria *(mandatory)*

- **SC-001**: All eight firmware builds link and are at or below 95% of their app slot.
- **SC-002**: `test_board_profiles` passes; no default pin of any profile is outside its usable set.
- **SC-003**: An S3 board can be installed and updated from a release without manual file picking.
- **SC-004**: Each board has passed a hardware check (boot, sensors, WiFi, OTA) on a real unit.

## Open questions

- The travel rig may use the ESP32-S3-Zero-N8R8 (8 MB, env `esp32s3-zero8`) instead of the 4 MB
  Zero. The 8 MB part has a 3.5 MB app slot (the 4 MB Zero has 1.75 MB, the tightest budget).
  Owner deciding; pins are the same on both.

## Assumptions

- Arduino-ESP32 3.x (spec 027) is the platform for all boards.
- Board choice is a build-time choice (one image per board); no runtime board detection.
