# Feature Specification: Environment and Cloud Cover

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Know whether it's clear (Priority: P1)

The astronomer sees an estimate of cloud cover from how much colder the sky is than the air,
and a plain condition label.

**Why this priority**: Cloud cover drives imaging decisions, safety and alerts.

**Independent Test**: With the IR sensor pointing at a clear sky, cloud cover reads near 0% and
the label says clear; under overcast it approaches 100%.

**Acceptance Scenarios**:

1. **Given** the IR sensor works, **When** readings arrive, **Then** the device reports sky
   temperature, ambient temperature, the raw and humidity-corrected sky-minus-ambient delta,
   cloud cover % and a condition (clear / cloudy / overcast) from the configured thresholds.
2. **Given** the corrected delta is between the clear and overcast thresholds, **When** cloud
   cover is computed, **Then** it scales linearly from 0% to 100%.
3. **Given** the dashboard, **When** it labels the cloud condition, **Then** the label agrees
   with the device's condition and the user's configured thresholds.

---

### User Story 2 - See the local environment (Priority: P2)

The astronomer sees temperature, humidity, pressure and dew point.

**Why this priority**: Dew and humidity decide whether optics fog and whether to close up.

**Independent Test**: Compare the readings with a reference thermometer/hygrometer; dew point
follows the Magnus formula.

**Acceptance Scenarios**:

1. **Given** the environment sensor works, **When** readings arrive, **Then** temperature,
   humidity, pressure (hPa) and dew point are reported and shown with consistent units.
2. **Given** the environment sensor is missing, **When** cloud cover is computed, **Then** a
   fixed 53% humidity is assumed, and both the API and the dashboard say the value is assumed.

---

### User Story 3 - Tune the cloud model (Priority: P3)

The user adjusts the clear/overcast thresholds and the humidity correction for their climate.

**Why this priority**: The heuristic depends on climate; defaults won't suit everyone.

**Independent Test**: Change a threshold in settings and see cloud cover recomputed.

**Acceptance Scenarios**:

1. **Given** settings, **When** the thresholds or correction factor change, **Then** the next
   readings use them; invalid combinations (clear ≥ overcast) are rejected.

### Edge Cases

- IR sensor missing: no cloud card, cloud-based rules and Alpaca properties report "no data"
  rather than a computed 100%.
- Humidity out of 0–100: clamped before correction.
- Environment sensor missing: humidity/dew-point Alpaca properties return an error, not the
  assumed 53%.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST report temperature (°C), relative humidity (%), pressure (hPa) and
  dew point (°C, Magnus formula) from the environment sensor, with a status and age.
- **FR-002**: The device MUST report IR sky and ambient temperatures with a status and age.
- **FR-003**: Cloud cover MUST be computed from the humidity-corrected sky-minus-ambient delta:
  0% at or below the clear threshold, 100% at or above the overcast threshold, linear between.
- **FR-004**: The device MUST classify the condition from the same thresholds and expose it.
- **FR-005**: Without the environment sensor, cloud cover MUST use an assumed 53% humidity and
  report that the humidity is assumed; the dashboard MUST show this.
- **FR-006**: The thresholds and correction factor MUST be configurable (defaults −13 °C, −3 °C,
  0.75) with validation.
- **FR-007**: The dashboard MUST show environment, cloud and IR cards only for sensors that are
  working, with the cloud label taken from the device's condition, and units shown consistently
  (°C, %, hPa).
- **FR-008**: These values MUST feed Alpaca ObservingConditions, MQTT, safety rules and alerts,
  and Alpaca MUST NOT report the assumed humidity as a measurement.
- **FR-009**: The documentation MUST describe the cloud model (delta, humidity correction,
  thresholds, fallback) consistently with the device.

### Key Entities

- **Environment reading**: temperature, humidity, pressure, dew point, status, timestamp.
- **IR reading**: object (sky) temperature, ambient temperature, status, timestamp.
- **Cloud metrics**: delta, corrected delta, cloud cover %, condition, description, humidity
  used and its source.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Cloud cover and condition match the documented model for 100% of tested deltas,
  including both threshold boundaries.
- **SC-002**: The dashboard's cloud label and the device's condition agree in 100% of cases.
- **SC-003**: Users can tell from the dashboard alone whether humidity is measured or assumed.
- **SC-004**: Zero discrepancies between documented and actual defaults and formulas.

## Assumptions

- The cloud model is the AAG CloudWatcher-style heuristic; it is approximate, especially in
  humid or foggy conditions.
- Pressure is reported as measured (station pressure), not reduced to sea level.
