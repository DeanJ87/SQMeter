# Feature Specification: Two sky sensors (SQM and SQM-L)

**Feature Branch**: `spec/030-dual-sky-sensors`

**Created**: 2026-10-17

**Status**: Draft

## Context

SQMeter reads one TSL2591 and reports one sky quality value (`sky.sqm`, mag/arcsec²), built in spec 001.
The ESP32-S3 boards (hardware Rev A, SQMeter-Hardware) add a second I2C bus with its own JST-XH port,
because the TSL2591's address (0x29) is fixed and two can't share a bus. That makes room for a second
sky sensor, so one device can carry both of the readings observers compare:

- **SQM**: the existing wide-field reading (the reference build is a ~20° lens and baffle, ADR-001).
- **SQM-L**: a narrow, lensed reading that looks at a small patch of sky near the zenith. It is less
  affected by light domes near the horizon and usually reads a little darker than the wide sensor.

Every consumer today expects one sky value (REST, WebSocket, MQTT and Home Assistant, Alpaca, the
SafetyMonitor, BLE, alert templates, the dashboard). This spec adds the second sensor without breaking
any of them. The owner picks which reading is **authoritative**; the authoritative reading fills every
existing single-value field, and both readings are also published under their own names.

Out of scope: the board profile and pin map for the new ESP32-S3 boards (separate work); translating the
documentation site (spec 029); a third or further sky sensor.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Both readings on one device (Priority: P1)

An observer fits a wide sensor on the main I2C port and a lensed sensor on the second port, enables the
SQM-L in Settings → Sensors, and sees both readings, each with its own name, on the dashboard and in the
API.

**Why this priority**: it is the feature.

**Independent Test**: with two TSL2591s fitted and SQM-L enabled, `/api/sensors` returns both readings
under their own names with their own status, and the dashboard shows both; covering one sensor changes
only that sensor's reading.

**Acceptance Scenarios**:

1. **Given** a TSL2591 on each port and SQM-L enabled, **When** readings update, **Then** each sensor
   reports its own SQM, raw SQM, NELM, Bortle class, lux, gain and status, measured independently.
2. **Given** both sensors reporting, **When** the dashboard loads, **Then** the authoritative reading is
   the large value, labelled with its name ("SQM" or "SQM-L"), and the other reading is shown smaller
   beside it with the difference between them (SQM-L − SQM, in mag/arcsec²).
3. **Given** SQM-L is enabled but no sensor answers on the second port, **When** Settings → Sensors loads,
   **Then** the SQM-L shows as not detected, its card is absent from the dashboard, and its status in the
   API is a fault, not a reading.

---

### User Story 2 - Choose which reading counts (Priority: P1)

The owner of a site with a bright horizon prefers the SQM-L and sets it as the authoritative reading.
From then on Alpaca `skyquality`, the SafetyMonitor's minimum-SQM limit, NELM and Bortle, the BLE
advertisement, the MQTT `sqm` entity and `sky.sqm` all follow the SQM-L.

**Why this priority**: without it the second sensor is only extra data; with it the device can be
deployed with either reading as its headline value.

**Independent Test**: switch the authoritative reading between SQM and SQM-L and confirm every
single-value consumer listed in FR-008 changes source, and that each reports which sensor it used.

**Acceptance Scenarios**:

1. **Given** a device with one TSL2591 (any existing device), **When** it updates to this firmware,
   **Then** the authoritative reading is the SQM and every existing field, topic, entity and Alpaca value
   is unchanged.
2. **Given** both sensors reporting and the authoritative reading set to SQM-L, **When** a client reads
   `sky.sqm`, Alpaca `skyquality` or the BLE summary, **Then** it gets the SQM-L value, and the source is
   reported as `sqml` (`sky.source`, Alpaca `SensorDescription`, BLE summary).
3. **Given** the SafetyMonitor's minimum-SQM limit enabled and the authoritative sensor faulted, **When**
   the verdict is evaluated, **Then** the device follows FR-010 and never reports safe on stale or missing
   sky data.

---

### User Story 3 - Calibrate each sensor on its own (Priority: P2)

The builder dark-calibrates and offsets each sensor against a reference meter separately, because the
two have different optics.

**Why this priority**: the two readings are only comparable once each is calibrated, but the device is
usable before that.

**Independent Test**: dark-calibrate the SQM-L with its aperture capped and confirm only the SQM-L's dark
offset changes; set an SQM offset on one sensor and confirm only that sensor's readings shift.

**Acceptance Scenarios**:

1. **Given** Settings → Sensors, **When** the owner opens a sky sensor's calibration, **Then** averaging
   window, SQM offset and dark calibration are set per sensor, with the same ranges and behaviour as
   spec 001 today.
2. **Given** dark calibration started for one sensor, **When** it completes, **Then** the other sensor's
   stored calibration is untouched.

---

### User Story 4 - Integrations see both (Priority: P2)

A Home Assistant user wants both readings on a chart; a script author wants both in the JSON; an alert
template wants to mention the SQM-L.

**Why this priority**: the value of a second reading is comparing the two over time, which happens
outside the device.

**Independent Test**: with MQTT discovery enabled and both sensors reporting, Home Assistant creates
separate SQM and SQM-L entities next to the existing authoritative `sqm` entity; an alert using
`{sqm_l}` renders the SQM-L value.

**Acceptance Scenarios**:

1. **Given** MQTT discovery on and SQM-L enabled, **When** discovery is published, **Then** entities for
   the authoritative SQM (existing id, unchanged), the wide SQM, the SQM-L, and the difference exist; a
   disabled sensor's entities are removed, not left unavailable forever.
2. **Given** an alert template, **When** it uses `{sqm}`, `{sqm_wide}`, `{sqm_l}` or `{sqm_source}`,
   **Then** each renders the authoritative value, the wide value, the narrow value and the source name.

### Edge Cases

- Only the second port has a sensor: the SQM is not detected; if SQM-L is enabled and set authoritative
  it fills the single-value fields, otherwise FR-010 applies.
- Both sensors fitted but the owner disables one: the disabled sensor is not read, has no card, no
  entities, and is absent from `skySensors`; if it was authoritative, FR-010 applies until the owner picks
  the other.
- The two readings differ by more than 1.5 mag/arcsec² (sensor swapped, lens fogged, one in sunlight):
  both are still reported; the dashboard shows the difference without hiding either.
- The second port is not wired on this board (classic ESP32 boards): SQM-L can't be enabled and the
  setting says why (settings dependencies, FR-013).
- Daylight and saturation: each sensor auto-ranges and reports saturation on its own; one saturated
  sensor doesn't affect the other's status.
- A sensor is unplugged while running: it goes to a fault status within one read interval and recovers
  without a restart when plugged back in.

## Clarifications

### Open questions (owner, 2026-10-17)

- **Q1 (FR-010)**: when the authoritative sensor is faulted or disabled and the other sensor is
  reporting, should the single-value fields fall back to the other sensor? [NEEDS CLARIFICATION:
  fall back automatically (flagged as a fallback), or report no sky reading until the owner switches?]

## Requirements *(mandatory)*

### Functional Requirements

**Sensors and settings**

- **FR-001**: The device MUST support up to two TSL2591 sky sensors: **SQM** on the main I2C bus and
  **SQM-L** on the second I2C bus. Each is read, auto-ranged, averaged and calibrated independently,
  with the behaviour spec 001 defines for the single sensor today.
- **FR-002**: Each sky sensor MUST have its own enable setting. SQM is enabled by default; SQM-L is
  disabled by default. A disabled sensor is not initialised or read.
- **FR-003**: Each sky sensor MUST have its own averaging window, SQM offset and dark calibration, with
  the ranges spec 001 sets. The existing settings become the SQM's settings with their current values.
- **FR-004**: The second I2C bus's SDA and SCL pins MUST be settable alongside the existing I2C pins and
  MUST NOT overlap any other configured pin (the existing pin-conflict checks apply).
- **FR-005**: The owner MUST be able to choose the authoritative sky reading: **SQM** (default) or
  **SQM-L**. The choice is only offered when both sensors are enabled.
- **FR-006**: The owner MUST be able to choose whether the dashboard shows the non-authoritative reading
  (default: shown).

**What is emitted**

- **FR-007**: Every existing single-value sky field MUST carry the authoritative reading, keep its name,
  type and units, and gain a source indicator (`sqm` or `sqml`):
  - REST `/api/sensors` and WebSocket `/ws/sensors`: the `sky` and `light` objects; add `sky.source`.
  - Alpaca ObservingConditions `skyquality` and `skybrightness`; `SensorDescription` names the sensor
    (for example "TSL2591 SQM-L (narrow)").
  - SafetyMonitor minimum-SQM rule, its reasons naming the sensor.
  - NELM and Bortle class.
  - BLE advertisement `sqm`; BLE summary `sqm` plus `sqmSource`.
  - MQTT `<base>/state` `sky.*` and the existing Home Assistant `sqm`, `nelm`, `bortle` and
    `illuminance` entities.
  - Alert template variables `{sqm}` and `{sqm_min}`.
- **FR-008**: Each enabled sky sensor's full reading MUST also be published under its own name, whichever
  is authoritative:
  - REST and WebSocket: a `skySensors` object with `sqm` and `sqml` entries (present only for enabled
    sensors), each holding `status`, `ageMs`, `sqm`, `rawSqm`, `nelm`, `bortle`, `description`,
    `calibrated`, `averagingWindowSeconds`, `lux`, `gain`, `integrationMs`, `saturated` and `nightMode`,
    plus `deltaSqm` (SQM-L − SQM) when both report.
  - MQTT `<base>/state`: the same `skySensors` object; Home Assistant entities "SQM (wide)",
    "SQM-L (narrow)" and "SQM-L − SQM", published only for enabled sensors and removed when disabled.
  - Alert template variables `{sqm_wide}`, `{sqm_l}`, `{sqm_delta}` and `{sqm_source}`.
  - `/api/status`: per-sensor health and diagnostics (detected, last update, gain, saturation).
- **FR-009**: The dark-calibration route MUST take the sensor to calibrate (`sqm` or `sqml`); a request
  without it calibrates the SQM, as today.
- **FR-010**: When the authoritative sensor is disabled, faulted or stale, the single-value fields MUST
  follow the owner's decision in Clarifications Q1, and in every case the SafetyMonitor MUST treat a
  missing authoritative reading as unsafe when the minimum-SQM rule is enabled (constitution I), with a
  reason naming the sensor.
- **FR-011**: The OpenAPI description (`docs/api/openapi.json`, from `tools/api`), the JSON schemas the
  contract tests use, and the demo device MUST describe and emulate both sensors; the demo shows an SQM-L
  reading slightly darker than the SQM.

**UI**

- **FR-012**: The dashboard Sky card MUST show the authoritative reading as the large value with its name
  chip, and, when FR-006 allows and the other sensor reports, the other reading and the difference in
  smaller type. The at-a-glance view (spec 025) MUST show a tile per enabled sensor. A disabled or
  undetected sensor has no card or tile (constitution V).
- **FR-013**: Settings → Sensors MUST list SQM and SQM-L with detected/not-detected state, enable switch,
  calibration and averaging per sensor, the authoritative choice and the dashboard option. Settings that
  can't apply (SQM-L on a board without a second bus, the authoritative choice with one sensor) MUST be
  shown disabled with a reason and link, through the settings dependencies catalogue (spec 020).

**Compatibility, budgets, docs, languages**

- **FR-014**: A device with one sensor and an existing saved configuration MUST behave and emit exactly as
  before after updating, with no factory reset. Configuration saved before this feature MUST load, with
  missing keys taking their defaults.
- **FR-015**: The feature MUST stay within the resource budgets (constitution IV): firmware slot, saved
  configuration size limit, and UI size growth.
- **FR-016**: The documentation MUST describe both sensors in the same change (constitution VII): the sky
  quality reference (what SQM and SQM-L measure and why they differ), configuration, REST, WebSocket,
  MQTT, Alpaca, BLE and alerts pages, the settings dependencies reference, and the hardware pages (the
  I2C-2 port).
- **FR-017**: Every new UI string MUST be added to the English catalogue with its context note and
  translated into all 13 other UI languages through the existing translation workflow, passing the
  translation checks.

### Key Entities

- **Sky sensor**: one TSL2591 with a role (`sqm` wide or `sqml` narrow), bus, enabled flag, calibration,
  averaging, detected state, and its latest reading and status.
- **Authoritative sky reading**: the reading every single-value consumer uses; has a source (`sqm` or
  `sqml`).
- **Sky difference**: SQM-L − SQM in mag/arcsec², only when both sensors report.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a device with one sensor, every existing API field, MQTT topic and entity, Alpaca value
  and BLE field is byte-for-byte unchanged in name and type after the update (contract tests pass
  unchanged), and ConformU still passes with zero errors.
- **SC-002**: With both sensors fitted, covering one sensor changes only that sensor's reading and leaves
  the other within its normal noise.
- **SC-003**: Switching the authoritative reading changes all single-value consumers in FR-007 within one
  read interval, and each reports the new source.
- **SC-004**: An owner can enable the SQM-L, calibrate it and make it authoritative in Settings in under
  3 minutes without consulting the docs.
- **SC-005**: Every new string appears in all 14 UI languages, and the translation and docs builds pass.

## Assumptions

- The second sensor uses the same optics guidance as ADR-001 but with a narrow lens; lens choice and the
  physical port are hardware matters (SQMeter-Hardware README).
- Names are fixed and translated ("SQM", "SQM-L"); there is no free-text label.
- The second bus exists only on boards that wire it; on others its pins are unset and SQM-L stays
  unavailable.
- Readings are not stored on the device (as today); comparison over time happens in integrations.

## Dependencies

- Spec 001 (sky quality), 013 (data interfaces), 016 (demo emulation), 020 (settings dependencies),
  023 (i18n), 025 (at-a-glance), 028 (OpenAPI).
- Board profiles for the ESP32-S3 boards (separate work) supply the second bus's default pins.
