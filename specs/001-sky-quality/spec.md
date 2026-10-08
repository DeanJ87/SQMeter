# Feature Specification: Sky Quality Measurement

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the existing sky quality measurement (SQM, NELM, Bortle, averaging and calibration) so convergence finds inconsistencies between code, documentation and UI."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See how dark the sky is (Priority: P1)

An astronomer opens the dashboard (or polls the API) at night and sees the sky brightness in
magnitudes per square arcsecond, stable enough to compare with a commercial SQM meter.

**Why this priority**: Sky brightness is what the device is for; everything else builds on it.

**Independent Test**: Under a dark sky, the dashboard and `GET /api/sensors` show an SQM value
that changes smoothly over minutes rather than jumping between samples.

**Acceptance Scenarios**:

1. **Given** a working light sensor at night, **When** the dashboard is open, **Then** it shows
   the SQM to two decimals, labelled mag/arcsec², and marks the data live or stale.
2. **Given** individual samples that fluctuate, **When** readings are reported, **Then** they
   are averaged over the configured window before conversion.
3. **Given** the light sensor isn't detected, **When** the dashboard loads, **Then** the sky card
   says the sensor isn't detected instead of showing numbers.

---

### User Story 2 - Understand the reading (Priority: P2)

The astronomer sees what the number means: naked-eye limiting magnitude, Bortle class and a
plain description ("Rural sky").

**Why this priority**: Most users think in Bortle classes and visible stars, not mag/arcsec².

**Independent Test**: Feed known SQM values and check NELM, Bortle class and description
against the documented table.

**Acceptance Scenarios**:

1. **Given** an SQM value, **When** it is reported, **Then** the Bortle class and description
   follow the documented SQM→Bortle table exactly, including at the boundaries.
2. **Given** an SQM below 15, **When** NELM is reported, **Then** it is 0.

---

### User Story 3 - Calibrate against a reference (Priority: P2)

The builder removes the sensor's dark floor (cap over the aperture) and applies an offset so the
reading matches a reference SQM-L.

**Why this priority**: Without calibration the absolute value isn't comparable between devices.

**Independent Test**: Cover the aperture, run dark calibration, and confirm the stored offset
is applied to the next readings; set an SQM offset and confirm the calibrated value shifts by it.

**Acceptance Scenarios**:

1. **Given** the aperture is covered and the averaging window has filled, **When** dark
   calibration is run, **Then** the current averaged visible count is stored as the dark offset,
   with the sample count and time, and survives restarts.
2. **Given** calibration is enabled with an SQM offset, **When** readings are reported,
   **Then** both the raw and the calibrated SQM are available and the headline value uses the
   calibrated one.
3. **Given** a user who doesn't use the command line, **When** they want to calibrate,
   **Then** they can run dark calibration and set the SQM offset and averaging window from the
   web UI.

---

### User Story 4 - Daylight and twilight don't break it (Priority: P3)

During the day the sensor saturates; the device keeps working and says so.

**Why this priority**: The device runs 24/7 outdoors.

**Independent Test**: Expose the sensor to daylight and confirm readings continue, saturation is
reported, and night-mode readings resume after dark.

**Acceptance Scenarios**:

1. **Given** bright light, **When** the sensor saturates, **Then** the reading is clamped,
   flagged as saturated, and the device auto-ranges to avoid saturation.
2. **Given** night returns, **When** readings resume, **Then** the night measurement mode
   (maximum sensitivity, longest integration) is used and flagged as night mode.

### Edge Cases

- Dark offset larger than the current signal: corrected count never goes below zero.
- Averaging window outside 10–300 s: clamped to the range.
- Lux at or below zero: clamped to a minimum before conversion so SQM stays finite.
- Calibration disabled: the offset is ignored but kept.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST report sky brightness in mag/arcsec² (higher = darker), derived
  from the light sensor using the conversion constant documented in the sky-quality reference.
- **FR-002**: The device MUST use a night measurement mode (maximum gain, longest integration)
  when dark and auto-range in daylight/twilight to avoid saturation, and MUST report which
  mode and whether the sensor is saturated.
- **FR-003**: Raw counts MUST be averaged over a configurable window (10–300 s, default 90 s)
  before conversion.
- **FR-004**: Users MUST be able to capture a dark offset; it MUST be subtracted from the
  averaged visible count, never producing a negative count, and MUST persist.
- **FR-005**: When calibration is enabled, an SQM offset MUST be applied; raw and calibrated SQM
  MUST both be reported.
- **FR-006**: NELM MUST be computed from SQM with the Unihedron formula and be 0 below SQM 15.
- **FR-007**: Bortle class (1–9) and its description MUST follow the documented SQM table.
- **FR-008**: The API MUST expose lux, raw counts, gain, integration time, averaging window,
  calibration state and sample diagnostics alongside the SQM, NELM and Bortle values.
- **FR-009**: The dashboard MUST show SQM, Bortle, NELM, illuminance and the description, and a
  trend of recent SQM readings that contains only real measurements.
- **FR-010**: Dark calibration, the SQM offset and the averaging window MUST be available from
  the web UI as well as the API; dark calibration requires the password when protection is on.
- **FR-011**: The SQM MUST feed the Alpaca ObservingConditions (sky quality, sky brightness),
  MQTT and the safety rules, using the same calibrated value everywhere.
- **FR-012**: The documentation (sky-quality reference, configuration reference, ADR-001) MUST
  state the same formulas, constants, defaults and ranges as the device.

### Key Entities

- **Light reading**: raw full/IR/visible counts, gain, integration time, lux, saturation and
  night-mode flags, timestamp.
- **Rolling average**: averaged counts over the window, sample count, rejected samples.
- **Calibration**: dark visible offset (with sample count and time), SQM offset, enabled flag.
- **Sky quality**: SQM (raw and calibrated), NELM, Bortle class, description.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Under a steady dark sky, successive reported SQM values vary by less than
  0.05 mag/arcsec² once the averaging window has filled.
- **SC-002**: Bortle class and NELM match the documented table/formula for 100% of tested values,
  including every class boundary.
- **SC-003**: A user can complete dark calibration in one action, and the new offset is
  reflected within one averaging window.
- **SC-004**: Zero discrepancies between the constants, defaults and ranges in the
  documentation and the device.
- **SC-005**: 24 hours of continuous operation through daylight produce no sensor errors other
  than reported saturation.

## Assumptions

- The reference optical build (20° lens with baffle, ADR-001) is used; other optics need their
  own calibration.
- "Night mode" is decided by the sensor's own auto-ranging, not by sun position.
- Calibration UI belongs on the Sensors settings tab; it's a rarely used, advanced operation.
