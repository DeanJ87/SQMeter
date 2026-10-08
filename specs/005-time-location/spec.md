# Feature Specification: Time, Location and Sun & Moon

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill time sources (NTP/GPS), time zone, location, darkness from the sun's position and the Sun & Moon card."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Correct time without thinking about it (Priority: P1)

The device keeps accurate local time from the internet or a GPS receiver, in the user's time zone.

**Why this priority**: Timestamps, daily resets, darkness and alerts all depend on it.

**Independent Test**: With NTP only, GPS only, and both (either priority), the device shows the
correct local time and survives losing one source.

**Acceptance Scenarios**:

1. **Given** NTP and/or GPS are enabled, **When** the device runs, **Then** it uses the primary
   source and falls back to the other; at least one source must stay enabled.
2. **Given** a time zone is chosen (list or custom POSIX string), **When** local times are shown
   or used, **Then** they follow it.

---

### User Story 2 - Darkness from where I am (Priority: P1)

The user enters coordinates (or has a GPS fix) and the device knows when it's dark at the chosen
level (sunset, nautical, astronomical).

**Why this priority**: "Only when it's dark" alerts depend on it.

**Independent Test**: Enter coordinates; the device's sun altitude and dark/not-dark state match
an ephemeris within 0.5°.

**Acceptance Scenarios**:

1. **Given** coordinates are saved or GPS has a fix, **When** status is read, **Then** the device
   reports the location source, sun altitude and whether it's dark; a GPS fix takes precedence.
2. **Given** the Alerts tab, **When** it shows "how dark it is now", **Then** the sun altitude and
   dark/not-dark state shown are the device's own (what alerts actually use), with predicted
   darkness start/end times alongside.
3. **Given** a secure (HTTPS) page, **When** the user taps "Use my location", **Then** the
   browser's location fills the coordinates; on plain HTTP the button isn't offered.

---

### User Story 3 - See tonight at a glance (Priority: P2)

The dashboard's Sun & Moon card shows twilight phase, tonight's darkness window, moon phase and
illumination, and a noon-to-noon chart with twilight bands, moon altitude and a hover readout.

**Why this priority**: Planning aid; not used for decisions.

**Independent Test**: Compare the card with an ephemeris for a known date and place.

**Acceptance Scenarios**:

1. **Given** a location, **When** the dashboard loads, **Then** the card shows phase, darkness
   window, moon phase/illumination and the chart; times are in the device's time zone (or clearly
   labelled as the browser's).
2. **Given** the card toggle is off or no location exists, **When** the dashboard loads, **Then**
   the card is hidden.

### Edge Cases

- No clock yet or no location: darkness is "unknown" and night-only rules don't hold alerts back.
- Polar summer/winter: "no astronomical dark tonight" rather than wrong times.
- Invalid coordinates: rejected (latitude ±90, longitude ±180).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST support NTP and GPS time sources with a primary/secondary order and
  require at least one enabled.
- **FR-002**: The device MUST apply a POSIX time zone chosen from a list or entered manually.
- **FR-003**: The device MUST store a manual location and prefer a GPS fix when present.
- **FR-004**: The device MUST compute the sun's altitude itself and report location source, sun
  altitude and darkness at the configured level.
- **FR-005**: Every darkness state shown to the user for alert purposes MUST come from the
  device; browser calculations are for predictions and charts only.
- **FR-006**: The Sun & Moon card MUST show twilight phase, darkness window, moon phase and
  illumination, rise/set times and a noon-to-noon chart, and be switchable off.
- **FR-007**: Times on the dashboard and settings MUST be in the device's time zone or labelled.
- **FR-008**: Every stored time setting MUST have an effect; settings that do nothing MUST NOT be
  stored or documented.
- **FR-009**: The configuration reference MUST describe the time and location settings as they work.

### Key Entities

- **Time settings**: NTP (servers, interval, time zone), GPS (pins, baud), source priority.
- **Location**: set flag, latitude, longitude, Sun & Moon card toggle.
- **Night state**: location source, known, is-night, sun altitude.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Sun altitude within 0.5° and rise/set/twilight times within 3 minutes of an
  ephemeris for tested dates and places.
- **SC-002**: The darkness state in the Alerts tab matches the device's state 100% of the time.
- **SC-003**: Zero stored-but-unused time settings.

## Assumptions

- Browser geolocation only works on HTTPS; the device serves plain HTTP.
