# Feature Specification: Dashboard, Demo and Screenshots

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Everything at a glance (Priority: P1)

The observer opens the device in a browser and sees live cards for safety, sky quality, sun & moon,
cloud, environment, GPS, light sensor, device & network, IR, wind and rain — only for hardware that
is enabled and working.

**Why this priority**: The dashboard is the device's face.

**Independent Test**: With some sensors disabled or unplugged, only the relevant cards appear and
data updates every second with a live/stale indicator.

**Acceptance Scenarios**:

1. **Given** a disabled or undetected sensor, **When** the dashboard loads, **Then** its data card
   isn't shown (the sky card shows "not detected" because sky quality is the core reading).
2. **Given** a live stream, **When** data stops arriving (the device flags its data old, or the
   stream sends nothing for 5 s), **Then** the sky card shows "Stale".

---

### User Story 2 - My layout (Priority: P2)

Cards flow into columns (Pinterest-style, 1–4 by width) and can be rearranged by drag or arrows;
the order persists in that browser.

**Acceptance Scenarios**:

1. **Given** Arrange mode, **When** a card is dragged or moved with arrows, **Then** the order
   updates, persists per browser, and Reset order restores the default.
2. **Given** a phone, **When** viewed, **Then** a single column with no horizontal scrolling.

---

### User Story 3 - Try it and read about it (Priority: P2)

A prospective user explores the public demo and reads documentation with up-to-date screenshots of
every page.

**Why this priority**: The demo and docs are how people evaluate and learn the device.

**Independent Test**: Open the demo and the docs; screenshots match the current UI, and demo
values are plausible for a real device.

**Acceptance Scenarios**:

1. **Given** the docs, **When** a user reads about the dashboard, settings, system, updates or
   Alpaca pages, **Then** a current screenshot and description are shown.
2. **Given** the demo, **When** values are shown, **Then** they are plausible (e.g. uptime in hours
   or days, not decades) and the live streams tick at the device's rates.

### Edge Cases

- No location: Sun & Moon card hidden.
- Alerts off: bell hidden; alerts paused: bell crossed out. *(Superseded by 021: alerts are paused
  and resumed, not switched off.)*

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The dashboard MUST show the listed cards only for enabled, working hardware, with
  live/stale state.
- **FR-002**: Cards MUST lay out in balanced columns by width (1 to 4) and be rearrangeable (drag,
  arrows, reset), persisting per browser.
- **FR-003**: The header MUST show the alerts bell when alerts are enabled, crossed out while they
  are paused (wording per spec 021).
- **FR-004**: The documentation MUST include a dashboard guide and current screenshots of each page,
  generated automatically from the demo.
- **FR-005**: The demo MUST use plausible mock data and the device's stream rates.

### Key Entities

- **Card**: id, title, visibility rule, saved position.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every page in the web UI has a current screenshot in the docs.
- **SC-002**: 0 implausible values in the demo (checked against realistic ranges).
- **SC-003**: Layout has no horizontal scroll at 400 px width.

## Assumptions

- Card order is a per-browser preference, not a device setting.
