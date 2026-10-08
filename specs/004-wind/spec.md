# Feature Specification: Wind (Anemometer and Vane)

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the optional cup anemometer and wind vane: speed, gust, direction, settings and safety limits."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Wind speed and gusts (Priority: P1)

The owner fits a cup anemometer and sees wind speed and gusts in standard definitions.

**Why this priority**: Wind is a roof/scope safety input.

**Independent Test**: Spin the anemometer at a known rate and compare speed and gust with the
calibration factor.

**Acceptance Scenarios**:

1. **Given** the anemometer is enabled, **When** pulses arrive, **Then** speed is the mean of the
   last 2 minutes and gust the highest 3-second mean in the last 10 minutes, in m/s (and km/h on
   the dashboard).
2. **Given** a known sensor model, **When** the user picks it, **Then** the matching calibration
   (2.4 or 3.621 km/h per Hz) is used; a custom value can be entered.

---

### User Story 2 - Wind direction (Priority: P2)

The owner fits a vane and sees direction.

**Why this priority**: Useful context; not a safety input.

**Independent Test**: Point the vane at each compass point; the reported direction matches,
with the north offset applied.

**Acceptance Scenarios**:

1. **Given** the vane is enabled, **When** readings arrive, **Then** direction is the
   speed-weighted circular mean over 2 minutes, and 0 when calm.
2. **Given** open or shorted vane wiring, **When** no position matches for 10 s, **Then** a vane
   fault is reported.

---

### User Story 3 - Wind safety limits (Priority: P2)

The owner sets maximum speed and gust; exceeding either makes the observatory unsafe
(feature 006).

**Acceptance Scenarios**:

1. **Given** limits are set, **When** speed or gust reaches them, **Then** the verdict is unsafe
   with the value and limit in the reason; the dashboard highlights a value that is over its
   configured limit.

### Edge Cases

- Pins conflicting with I2C, GPS or the rain sensor are rejected.
- Vane pin outside ADC1 (GPIO 32–39) is rejected.
- Contact bounce faster than 2 ms is ignored.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST measure wind speed (2-minute mean) and gust (max 3-second mean over
  10 minutes) from anemometer pulses, debounced at 2 ms, with a configurable km/h-per-Hz factor.
- **FR-002**: The device MUST measure direction from the vane (16 positions, configurable
  pull-up and north offset) as a speed-weighted circular mean, 0 when calm, and report vane faults.
- **FR-003**: Settings MUST validate pins (no conflicts, vane on ADC1) and apply without a restart.
- **FR-004**: Wind MUST feed Alpaca (`windspeed`, `windgust`, `winddirection` in m/s and degrees),
  the dashboard, MQTT and the safety limits.
- **FR-005**: The dashboard MUST show a wind card only when the anemometer is enabled, and
  highlight values against the user's configured limits.
- **FR-006**: The documentation MUST name the settings locations and labels as they appear in the UI.

### Key Entities

- **Wind reading**: speed, gust, instantaneous speed, direction, direction validity, vane fault,
  sample count, age.
- **Wind settings**: enabled, pins, calibration factor, vane enabled, pull-up, north offset.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Speed, gust and direction match the documented definitions for 100% of simulated
  pulse/vane sequences.
- **SC-002**: Every settings location named in the docs exists in the UI under that name.

## Assumptions

- One anemometer and one vane per device.
