# Feature Specification: Demo Conditions

**Feature Branch**: `feat/demo-conditions` (to be created when implementation starts)

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "The demo's Demo panel should let you set conditions directly instead of only canned scenarios: set the device date/time (with presets, e.g. Dec 31 at the North Pole, midsummer midnight, tonight's darkest moment, dawn - Dawn must move the clock to before sunrise rather than faking light), set location presets (e.g. London, Atacama, high Arctic like 75,-1, Sydney, La Palma), rain on/off and rain rate, sky brightness/Bortle (target SQM), cloud state (clear, clouding over, clearing, overcast, broken/patchy), wind, sensor faults; combinations of conditions persist and compose; scenarios become presets of conditions. Respect device settings (rain controls disabled when the rain sensor is off, with link). Values are fed as raw simulated sensor inputs so the firmware's own logic derives everything; sensor averaging means changes take some seconds - the UI should show target vs current. Must remain nothing-outbound, session-only, and mobile-friendly (the panel must not cover page controls like Save buttons on phones)."

**Related**: [spec 016 - Demo that behaves like the device](../016-demo-device-emulation/spec.md) (the emulated device, FR-006 nothing outbound, FR-008 session-only state, FR-009 scenarios); spec 018 - demo tour and opt-in notifications (its tour steps will use these conditions).

## Background

Today the Demo panel offers fixed scenarios (Night sky, Rain, Cloud over, Clear, Dawn, sensor
fails). Each one replaces the previous one, runs for a fixed time and then reverts. Visitors
reported that:

- **Dawn** only brightens the light sensor; the device's clock, sun and moon don't move to dawn, and
  the device's light averaging hides much of the change.
- You can't combine conditions, for example rain on a moonlit night in the high Arctic.
- You can't pick a date or place to see what the device does there, for example 31 December at the
  North Pole.

The firmware's own logic (compiled for the browser, spec 016) already turns raw sensor values into
every derived reading and decision. This feature changes only *what the simulated sensors report*
and *the device's clock*. It does not change the logic.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Set the sky's conditions and combine them (Priority: P1)

A visitor opens the Demo panel and sets individual conditions: cloud (clear, patchy, clouding over,
clearing, overcast), rain (off, or on at a chosen rate), sky brightness (as a Bortle class or a
target SQM), wind (calm to gale, with gusts) and sensor faults (one per sensor). Each condition stays
as set until changed. Conditions combine, and the dashboard, safety verdict, alerts and Alpaca all
react the way a real device would.

**Why this priority**: Visitors can't explore the device's behaviour today. Composable conditions
are the core of the request, and every other story builds on them.

**Independent Test**: Set cloud to overcast and rain to 3 mm/h together. Within the settling time,
the dashboard shows rain and full cloud cover, the verdict is unsafe with both reasons, and Alpaca
IsSafe is false. Then set rain off: the rain reason clears after the device's rain clear delay, and
the cloud reason remains.

**Acceptance Scenarios**:

1. **Given** a clear night, **When** the visitor sets cloud to "overcast", **Then** cloud cover rises
   and passes the device's cloud limits, and the verdict turns unsafe with a cloud reason.
2. **Given** cloud "overcast", **When** the visitor also sets rain on, **Then** both conditions
   apply at once and setting one does not reset the other.
3. **Given** rain on, **When** the visitor changes the rain rate, **Then** the device's rain rate
   reading moves toward the new rate.
4. **Given** a dark sky, **When** the visitor picks Bortle 8 (or a target SQM of 18.0), **Then** the
   device's SQM settles near the target, and its Bortle and NELM follow from the device's own
   calculation.
5. **Given** any conditions, **When** the visitor sets a sensor fault, **Then** that sensor stops
   answering until the fault is cleared, with no time limit.
6. **Given** the rain sensor is switched off in the device's settings, **When** the visitor opens
   the panel, **Then** the rain controls and rain-sensor fault are unavailable, with the reason and a
   link to Settings → Sensors.

---

### User Story 2 - Choose the date, time and place (Priority: P1)

A visitor sets the device's date and time and its location, either exactly or from presets. The
sun, moon, darkness, sky brightness and every time-dependent feature follow.

Date and time presets:
- now
- tonight's darkest moment
- dawn (before sunrise)
- dusk
- midsummer midnight
- midwinter midnight
- 31 December

Location presets:
- London
- La Palma
- Atacama
- Sydney
- the high Arctic (75° N, 1° W)
- the North Pole

**Why this priority**: Visitors reported that Dawn didn't actually go to dawn and that they can't see
the device elsewhere or at another time. Time and place drive darkness, Sun & Moon, the night alerts
and the light level, so they must be real.

**Independent Test**: Pick North Pole and 31 December. The device reports the sun well below the
horizon all day, Sun & Moon shows no sunrise, and darkness reads as continuous. Then pick "dawn":
the device clock moves to before sunrise there (or reports that there is no sunrise), and as the
clock runs, the sky brightens from the device's real sun position.

**Acceptance Scenarios**:

1. **Given** any time, **When** the visitor picks "dawn", **Then** the device clock moves to a set
   time before sunrise at the device's location (default 45 minutes), and the light level then
   follows the real sun as the clock runs.
2. **Given** a location with no sunrise on that date (polar night) or no darkness (midnight sun),
   **When** a preset that needs one is picked, **Then** the panel says why it can't apply and the
   clock does not jump.
3. **Given** any time, **When** the visitor enters an exact date and time, **Then** the device clock
   is set to it and keeps running from there, at 1× or 10×.
4. **Given** the visitor picks a location preset, **When** it is applied, **Then** the device's
   saved location changes, the device's time zone changes to the place's, and Sun & Moon, darkness
   and the device's sun altitude all follow, exactly as when the location is changed in Settings.
5. **Given** GPS is on, **When** the visitor picks a location, **Then** the simulated GPS reports
   that place, because GPS outranks the saved location on the device.

---

### User Story 3 - See what has been set and what the device reads now (Priority: P2)

The device averages and smooths its readings, so a condition takes some seconds to show. For each
condition, the panel shows the target the visitor set next to what the device currently reads, for
example "Cloud: overcast (target 100%) - device reads 63%". It also shows the device's date, time,
time zone and location.

**Why this priority**: Without it, visitors think a condition did nothing (as reported for Cloud
over and Dawn) when the device is simply settling.

**Independent Test**: Set rain to 5 mm/h. The panel immediately shows the target 5 mm/h and the
device's current rain rate, which converges to it.

**Acceptance Scenarios**:

1. **Given** a condition has just changed, **When** the device has not settled yet, **Then** the
   panel shows the target and the current reading, with a "settling" note.
2. **Given** the device has settled, **When** the panel is open, **Then** the target and current
   reading agree within a stated tolerance and the note goes away.

---

### User Story 4 - Scenarios as presets (Priority: P2)

The existing scenarios remain as one-click presets. Each preset sets a group of conditions:
- Night sky: darkest moment tonight, clear, dark sky.
- Rain shower: rain at 2.4 mm/h, overcast.
- Cloud over / Clearing: cloud set to clouding over or clearing.
- Dawn: the clock goes to dawn.
- Sensor fails: one sensor fault.

After applying a preset, the visitor can adjust any single condition. Timed presets such as "a
shower that stops" return their conditions to the previous values when they end.

**Why this priority**: Presets keep the quick path that docs links (`?scenario=`) and the tour
(spec 018) rely on.

**Independent Test**: Open `?scenario=rain`. The rain shower preset applies, and the panel shows
rain on and cloud overcast as individual conditions that the visitor can then change.

**Acceptance Scenarios**:

1. **Given** the demo URL has `?scenario=<id>` for any existing scenario, **When** it loads, **Then**
   the matching preset applies.
2. **Given** a preset is applied, **When** the visitor changes one condition, **Then** only that
   condition changes.
3. **Given** a timed preset ends, **When** its time runs out, **Then** its conditions revert and
   the panel shows the change.

---

### User Story 5 - The panel works on a phone (Priority: P2)

On a phone, the Demo button and panel never cover the page's own controls, such as a settings tab's
Save button. The open panel can be scrolled and closed with one tap.

**Why this priority**: The floating Demo button currently covers the Save button on mobile, which
blocks the main thing visitors try (changing settings).

**Independent Test**: At 375×667, open every settings tab and scroll to the bottom. Every Save
button can be tapped with the panel closed. Then open the panel and set a condition without the
page scrolling horizontally.

**Acceptance Scenarios**:

1. **Given** a phone-sized screen, **When** a page has controls at the bottom, **Then** the page
   leaves room for the Demo button, so no control sits under it.
2. **Given** the panel is open on a phone, **When** it is taller than the screen, **Then** it
   scrolls within itself and its close control stays reachable.

---

### Edge Cases

- Polar night and midnight sun: presets that need a sunrise, sunset or darkness say why they can't
  apply rather than jumping somewhere arbitrary (US2-2).
- The rain sensor is switched off while it is raining: rain stops being simulated, and the rain
  controls become unavailable. When the sensor is switched back on, rain returns to "off" rather
  than resuming silently.
- A sensor fault is set on a sensor the device has switched off (rain, wind): no fault control is
  offered for it.
- A target SQM brighter than the sun allows (asking for Bortle 1 at noon): the panel shows that the
  sun outshines the target. The device reads what the sun gives and does not report a false dark
  sky.
- The device restarts (demo restart or GPS change): conditions persist across the restart, as
  weather would.
- 10× speed with a timed preset: the preset's duration runs on the device clock, so it is 10×
  shorter in wall time.
- The visitor enters an invalid date or time, or one outside the range the device can represent:
  the input is rejected with a message, and the clock does not change.
- "Reset demo" returns all conditions, the clock and the location to defaults.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Demo panel MUST let the visitor set each condition independently: cloud state
  (clear, patchy/broken, clouding over, clearing, overcast), rain (off, or on with a rate in mm/h),
  sky brightness (Bortle class 1-9 or target SQM in mag/arcsec²), wind (speed, with gusts and
  direction) and per-sensor faults.
- **FR-002**: Conditions MUST compose: setting one condition MUST NOT change another, and every set
  condition stays in force until changed or until a timed preset that set it ends.
- **FR-003**: Conditions MUST be applied only as raw simulated sensor inputs (and the device clock
  and location). Every derived value MUST come from the device's own logic, as in spec 016 FR-004:
  - SQM, NELM, Bortle and cloud cover
  - dew point
  - the safety verdict and its reasons
  - alerts
  - Alpaca
- **FR-004**: The panel MUST let the visitor set the device's date and time exactly, and offer
  presets:
  - now
  - tonight's darkest moment
  - dawn (a set time before sunrise, default 45 minutes)
  - dusk
  - midsummer midnight
  - midwinter midnight
  - 31 December 23:00

  The clock MUST keep running from the set time at the chosen speed (1× or 10×).
- **FR-005**: Dawn MUST move the device clock to before sunrise at the device's location. The light
  level MUST then follow the real sun position as the clock runs. Faking light levels for dawn MUST
  be removed.
- **FR-006**: The panel MUST offer location presets: London, La Palma, Atacama, Sydney, the high
  Arctic (75° N, 1° W) and the North Pole.
  - Applying one MUST change the device's saved location and time zone the same way saving them
    in Settings does.
  - The simulated GPS, when on, MUST report the chosen place.
- **FR-007**: Where a time preset cannot apply at the current place and date (no sunrise, no
  darkness), the panel MUST say why and leave the clock unchanged.
- **FR-008**: Controls that depend on a sensor the device has switched off MUST be unavailable,
  showing the reason and a link to the setting. This applies to the rain controls, the rain-sensor
  fault and the wind controls.
- **FR-009**: For each condition, the panel MUST show the target and the device's current reading
  of it, and indicate "settling" until they agree within a tolerance.
- **FR-010**: The panel MUST show the device's date, time, time zone and location.
- **FR-011**: The existing scenarios MUST remain available as presets that set groups of conditions.
  `?scenario=<id>` links (night, rain, cloud, clear, dawn, fail-light, fail-ir, fail-environment,
  fail-rain) MUST keep working.
- **FR-012**: Timed presets MUST run on the device clock and, when they end, revert only the
  conditions they set.
- **FR-013**: Conditions, the clock and the location MUST persist for the browser session,
  including through a page refresh and an emulated restart, and MUST be cleared by "Reset demo"
  (spec 016 FR-008).
- **FR-014**: Nothing in this feature may make a network request beyond the demo's own files
  (spec 016 FR-006). Location presets and time zone data MUST be bundled.
- **FR-015**: On phone-sized screens, the Demo button and panel MUST NOT cover any page control.
  The open panel MUST scroll within itself and keep its close control reachable.
- **FR-016**: Every control in the panel MUST be usable by keyboard and labelled for screen readers.
- **FR-017**: The docs' Live Demo page MUST describe the conditions, presets and links.

### Key Entities

- **Condition**: one adjustable aspect of the simulated world (cloud, rain, sky brightness, wind,
  sensor fault), with a target value, the time it was set, and optionally the preset that set it.
- **Device clock**: the emulated device's date and time. It can be set and runs at 1× or 10×.
- **Location preset**: a named place with latitude, longitude, elevation and time zone.
- **Preset**: a named group of conditions, optionally with a clock or location change and a
  duration. The existing scenarios become presets.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every combination of two conditions from different groups (cloud, rain, brightness,
  wind, faults) can be set at once, and the device reflects both within 60 seconds at 1×.
- **SC-002**: After a condition is changed, the device's reading reaches the target (within
  tolerance) within 60 seconds at 1×, and the panel shows target and current throughout.
- **SC-003**: "Dawn" at London on any date puts the device's sun between 6° and 12° below the
  horizon and rising. At the North Pole on 31 December, the panel explains that there is no sunrise.
- **SC-004**: All existing `?scenario=` links produce the same visible outcome as before:
  - rain: unsafe with "Rain detected"
  - cloud: unsafe with a cloud reason
  - night: dark sky and "Dark now"
- **SC-005**: At 375×667, every Save button on every settings tab can be tapped with the Demo button
  present.
- **SC-006**: An automated run that exercises every control and preset records zero requests
  leaving the demo's origin.

## Assumptions

- The demo stays a browser-only, single-visitor emulation (spec 016). Conditions are per tab and are
  not shared.
- Patchy/broken cloud varies cover around about 50% over a few minutes, so the device's averaging and
  the cloud alerts see realistic movement.
- Wind controls apply only when the anemometer is on in Settings. Wind faults are not separately
  offered (spec 004 has no wind fault behaviour beyond "missing").
- Sky brightness is set as a target zenith brightness for a moonless, sun-free sky. The moon and sun
  still add their light, so the device's SQM is brighter than the target when either is up. The
  panel says so.
- Time zones for presets are bundled as fixed POSIX rules (the format the device's time zone setting
  already uses). A full time zone database is not needed.
- Settling tolerances: cloud ±5%, rain ±0.2 mm/h, SQM ±0.2, wind ±0.5 m/s.
- The exact layout of the panel (sections, collapsible groups, sheet vs floating on phones) is a
  planning decision. Requirements constrain only behaviour and reachability.
- The tour in spec 018 will drive these conditions. This spec does not define the tour.
