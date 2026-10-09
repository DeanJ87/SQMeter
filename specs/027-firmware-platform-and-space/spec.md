# Feature Specification: Firmware platform and flash space

**Feature Branch**: `spec/027-firmware-platform`

**Created**: 2026-10-09

**Status**: Draft – research done ([research.md](research.md)), for the owner's review. Nothing built.

## Context

The firmware is close to full: the standard build uses 94.8% of its 1.5 MB app slot and the
Bluetooth (BLE) build 97.2% of its 1.69 MB slot. It is built on Arduino-ESP32 2.0.17 (ESP-IDF
4.4.7), which is end-of-line. Arduino-ESP32 3.x (ESP-IDF 5.5) brings IPv6 for TLS and time sync
(the gap spec 015 had to leave), TLS 1.3 and maintained libraries, but its images are larger.

This spec covers three things that have to be decided together:

1. **Space**: know where the flash goes, trim what's safe, and stop being surprised (a CI budget).
2. **Platform**: move to Arduino-ESP32 3.x.
3. **Upgrade path**: get existing devices onto 3.x without losing settings, the web UI or the
   language file, and without a trip to every device unless that is a deliberate choice.

The research spike measured all of this (both builds compiled and linked on 3.x; nothing flashed).
Headline numbers:

| Build | 2.x today | 2.x trimmed | 3.x trimmed, current partitions | 3.x trimmed, whole-chip layout |
|---|---|---|---|---|
| Standard | 94.8% | 94.4% | 99.4% | 85.2% |
| BLE | 97.2% | 96.6% | 99.6% | 96.0% |

## User Scenarios & Testing *(mandatory)*

### User Story 1 – A size budget that warns before it's too late (Priority: P1)

A contributor opens a PR that grows the firmware. CI shows each build's size against its app slot
and fails the PR if a build passes the limit, so nobody finds out on release day that an update no
longer fits.

**Why this priority**: The BLE build has ~50 KB left. Without a budget, any feature can silently
make a release unflashable.

**Independent Test**: Open a PR that adds a large constant array; CI reports the size change per
build and fails above the limit.

**Acceptance Scenarios**:

1. **Given** a PR, **When** CI builds both firmware variants, **Then** the PR shows each build's
   size, its slot, the percentage used and the change from the target branch.
2. **Given** a build above the warning level, **When** CI runs, **Then** it warns but passes.
3. **Given** a build above the failure level, **When** CI runs, **Then** it fails with the
   largest contributors listed.

---

### User Story 2 – Safe trims now (Priority: P1)

The owner gets back the space that costs nothing: release builds log errors only from the Arduino
core, NimBLE's own logging is off, and duplicate embedded certificates are removed.

**Why this priority**: Cheap and low-risk; it buys time for the platform move.

**Independent Test**: Build both variants before and after; compare sizes; run the device checks.

**Acceptance Scenarios**:

1. **Given** the trimmed build, **When** it runs on the spare, **Then** alerts, OTA, Bluetooth
   advertising and the web UI behave as before, and the device's own log lines still appear.

---

### User Story 3 – Firmware on Arduino-ESP32 3.x (Priority: P2)

The firmware builds and runs on Arduino-ESP32 3.x for both variants, with the same behaviour and
APIs as on 2.x, plus IPv6 for secure connections and time sync.

**Why this priority**: 2.x is end-of-line; 3.x is where fixes, TLS updates and IPv6 are.

**Independent Test**: Every existing test and device check passes on 3.x; ConformU shows no
errors; alerts reach ntfy over IPv6 where the network has it.

**Acceptance Scenarios**:

1. **Given** a 3.x build, **When** the full test suite and the spare-device checks run, **Then**
   they pass as on 2.x.
2. **Given** an IPv6-only path to ntfy or GitHub, **When** the device sends an alert or checks for
   updates, **Then** it works over IPv6.

---

### User Story 4 – Existing devices upgrade by OTA (Priority: P2)

A device on 2.x is updated to the 3.x release from its Updates page, as with any release. It keeps
its settings, its web UI and its language file. If the update fails, it comes back on the old
version.

**Why this priority**: The owner's main device is remote; a USB trip is expensive.

**Independent Test**: On the spare: OTA from the last 2.x release to 3.x, power-cut mid-update,
and OTA back to 2.x.

**Acceptance Scenarios**:

1. **Given** a device on the last 2.x release, **When** it installs the 3.x release over the air,
   **Then** it boots, keeps its settings and language, and its web UI loads.
2. **Given** power is cut while the firmware is written, **When** the device restarts, **Then** it
   runs the previous version.
3. **Given** a device on 3.x, **When** it is updated back to the last 2.x release, **Then** that
   release mounts the filesystem and keeps its settings.

---

### User Story 5 – Room to grow (Priority: P3)

After the move, both builds have meaningful free space again, using the flash that today's layout
leaves unused, at the cost of one USB flash per device, done when convenient and announced.

**Why this priority**: 3.x on today's layout leaves ~10 KB (standard) and ~7 KB (BLE), so the next
feature would not fit. But it costs a hands-on step, so it comes after the OTA path works.

**Independent Test**: Flash the new layout over USB on the spare; OTA works afterwards; settings
are restored from a backup.

**Acceptance Scenarios**:

1. **Given** a device on the new layout, **When** a release is installed over the air, **Then** it
   installs as before.
2. **Given** a device to be moved, **When** the owner follows the documented steps, **Then**
   settings come back from the exported backup.

### Edge Cases

- A 2.x device downloads the 3.x release and the firmware step fails after the web UI step: the old
  firmware must still mount the filesystem (research R4).
- A build's custom framework options silently don't apply (renamed between ESP-IDF versions, or a
  stale generated config): the build must fail, not ship a different configuration.
- WPA3-only networks: disabling WPA3 would save 27 KB but strand devices on those networks.
- A remote device can't be reached by USB: it stays on the current layout and keeps receiving OTA
  updates that fit it, for as long as the project supports that layout.
- Arduino-ESP32 4.0 (ESP-IDF 6.1) becomes final mid-migration: decide whether to target it.

## Requirements *(mandatory)*

### Functional Requirements

**Budget and visibility**

- **FR-001**: CI MUST report, for every PR and for both firmware builds, the image size, the app
  slot size, the percentage used and the change from the target branch.
- **FR-002**: CI MUST warn when a build uses more than 90% of its app slot and fail above 95%,
  listing the ten largest contributors (a size map like [tools/size_map.py](tools/size_map.py)).
  A justified overrun can be recorded per PR, like the UI size budget's reason (SIZE-01).
- **FR-003**: The budget rule MUST be in the coding standard (alongside SIZE-01..03) and the
  constitution's budget principle.

**Trims**

- **FR-004**: Release builds MUST log only errors from the Arduino core (`CORE_DEBUG_LEVEL=1`) and
  disable NimBLE's internal logging; the device's own log lines are unchanged.
- **FR-005**: Each root certificate MUST be embedded once.
- **FR-006**: Every trim MUST be verified on the spare: alerts (TLS), OTA, Bluetooth advertising and
  pairing, the web UI.

**Platform**

- **FR-007**: The firmware MUST build for both variants on a pinned Arduino-ESP32 3.x release
  (pioarduino platform, pinned version), with the strict warning flags applied to our own code.
- **FR-008**: Behaviour, APIs, settings and the web UI MUST be unchanged by the move, apart from
  IPv6 support for TLS connections and time sync (completing spec 015).
- **FR-009**: The IPv6 web listener MUST move onto the main server (the 3.x TCP layer listens on
  both protocols), keeping the rule that only local-network IPv6 peers are served (spec 015).
- **FR-010**: Framework options (`custom_sdkconfig`) MUST be checked after generation: every option
  the project sets is present with the intended value, and builds start from clean generated files.
- **FR-011**: The 3.x builds MUST keep WPA3 support unless the owner decides otherwise (research R3).

**Upgrade path**

- **FR-012**: A device on the last 2.x release MUST be able to install the first 3.x release over
  the air with the existing bootloader and partition table, keeping settings, web UI and language
  file.
- **FR-013**: The 3.x firmware MUST keep the filesystem readable by the last 2.x release (LittleFS
  on-disk version 2.0), and the transition release MUST write the firmware before the filesystem
  image, so an interrupted update never leaves a device without a web UI.
- **FR-014**: Each step MUST be proven on the spare device before the owner's main device: OTA up,
  power-cut during the update, OTA back down to 2.x.
- **FR-015**: The partition layout MUST NOT change unless the owner decides it in this spec's
  clarification. If it changes:
  - the new layout uses the whole 4 MB chip;
  - the move is documented step by step, including exporting and restoring settings;
  - releases keep supporting devices on the old layout until a stated date.
- **FR-016**: The last 2.x release MUST stay available as a fallback, with docs on how to return to it.

**Docs**

- **FR-017**: The OTA and update docs MUST describe the platform move, what's kept, how to go back,
  and (if FR-015 applies) the USB step.

### Key Entities

- **Firmware build**: variant (standard, BLE), platform version, image size, app slot size.
- **Partition layout**: app slot sizes, filesystem size and offset; changing it needs USB.
- **Size budget**: warning and failure levels per build; recorded overruns.

## Success Criteria *(mandatory)*

- **SC-001**: Every PR shows both firmware sizes; a PR that pushes a build over 95% fails.
- **SC-002**: Both builds run on Arduino-ESP32 3.x with all tests passing and no ConformU errors.
- **SC-003**: The spare goes 2.x → 3.x → 2.x over the air with settings, web UI and language intact,
  and survives a power cut during the update.
- **SC-004**: TLS alerts and the update check work over IPv6 on an IPv6 network.
- **SC-005**: After the move, each build has at least 5% of its slot free (needs FR-015 for BLE).

## Assumptions

- 4 MB flash on every device (as today). Larger-flash hardware is out of scope.
- The owner's main device runs the BLE build on `partitions_ble.csv`; the spare runs the standard
  build on `partitions.csv`.
- No secure boot or flash encryption is in use (none is configured).
- The migration targets Arduino-ESP32 3.3.x; 4.0 is reconsidered when final.

## Clarifications

### Session 2026-10-09

- Q: Change the partition layout (FR-015)? → A: **Yes.** This is a beta; a whole-chip reflash is
  acceptable if it is better long term. Move to the whole-chip layout (research R3) with the 3.x
  release. Requirement: **people's settings must be kept** across the reflash. The docs explain the
  steps.

This decision makes FR-015 apply and adds:

- **FR-018**: The layout change MUST keep each device's settings. Preferred: the reflash preserves
  the NVS partition (unchanged offset and size in the new layout), so settings survive without any
  user action. Fallback: export settings (including secrets, under the existing auth rules) before
  the reflash and restore them afterwards. Either way, the spare proves it before any other device.
- **FR-019**: A browser-based flasher or a documented `esptool` command MUST write the bootloader,
  the new partition table, both app slots' firmware and the filesystem in one step, without erasing
  NVS.

- Q: Keep WPA3 (FR-011)? → A: **Yes, keep it.** WPA3-only networks stay supported; the 27 KB stays
  spent.
- Q: Ship beta.4 on 2.x with the trims first? → A: **No.** The next release is the 3.x release on
  the whole-chip layout. Consequences: the trims (FR-004, FR-005) and the CI budget (FR-001..003)
  land as part of this work, not as a separate 2.x release; FR-016's "last 2.x release" fallback is
  v0.2.0-beta.3.
