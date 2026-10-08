# Feature Specification: Demo Conditions - Set the Sensor Readings

**Feature Branch**: `feat/demo-conditions` (to be created when implementation starts)

**Created**: 2026-10-08

**Revised**: 2026-10-08: the primary controls are now the raw sensor readings, not outcome labels.

**Status**: Draft

**Input**: User description:
- First version: "The demo's Demo panel should let you set conditions directly instead of only canned scenarios: set the device date/time (with presets …), set location presets …, rain on/off and rain rate, sky brightness/Bortle, cloud state (clear, clouding over, clearing, overcast …), wind, sensor faults …"
- Revision: "the demo params rely on predictions that we expect things haven't been adjusted and take time (which isn't obvious when adjusting) for example if i press cloud over but i have set the differential of clear to be -30c it wont ever reach clear. I think that's a big part of why we shouldn't have labels like this and instead allow setting differentials, or rather, set the ambient, set the sky temp, set the rain rate etc."

**Related**:
- [Spec 016: a demo that behaves like the device](../016-demo-device-emulation/spec.md): the emulated device, FR-006 (nothing outbound), FR-008 (session-only state) and FR-009 (scenarios).
- Spec 018: the demo tour and opt-in notifications. Its tour steps drive these controls.

## Background

Today the Demo panel offers fixed scenarios: Night sky, Rain, Cloud over, Clear, Dawn and sensor
faults. Each one is an *outcome label* backed by hidden assumptions about the device's settings.

**Cloud over** writes a sky temperature that the device's *default* cloud thresholds read as
overcast. Change the thresholds (for example, set the clear-sky differential to -30 °C) and
"Cloud over" or "Clear" may never be reached. Nothing in the panel says why.

The device also averages its readings and holds some states, so a change takes time to show:
- the sky brightness averaging window
- the rain clear delay
- the safe delay

That isn't visible either, so a working control looks broken.

The demo runs the firmware's own logic (spec 016), so the honest control is the one the real
hardware has: **what each sensor reports**. The visitor sets:
- the air temperature
- the sky temperature
- the rain rate
- and so on

The device then works out cloud cover, SQM, dew point and the verdict exactly as it would on the
roof. Named shortcuts such as "Overcast" or "Just unsafe" still exist. They **work out the sensor
readings from the device's current settings**, say which settings they used, and say so when a
target can't be reached.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Set what each sensor reports (Priority: P1)

The Demo panel has one control per raw reading the hardware produces:
- **Air (BME280)**: temperature, humidity and pressure.
- **Infrared (MLX90614)**: sky (object) temperature and the sensor's own ambient temperature, plus
  a linked **sky minus ambient** differential.
- **Light (TSL2591)**: illuminance in lux.
- **Rain (RG-15)**: rate in mm/h and a lens fault.
- **Wind**: speed, gust and direction.
- **GPS**: fix on or off, and position.
- **Every sensor**: a "not responding" fault.

Each value stays as set until changed, and changing one never changes another, except the linked
differential (FR-003). Everything the device derives from these readings comes from the
firmware's own logic.

**Why this priority**: Outcome labels hide assumptions that break when settings change. Raw
readings are exactly what the device receives, so every behaviour, including the visitor's own
thresholds, can be explored truthfully.

**Independent Test**: Set the clear-sky threshold in Settings to -30 °C. In the panel, set air
20 °C, sky -12 °C (a differential of -32 °C) and humidity 40%. The device reads clear sky. Raise
the sky temperature to -5 °C: cloud cover rises, as the device's formula gives for the visitor's
thresholds.

**Acceptance Scenarios**:

1. **Given** any settings, **When** the visitor sets the sky and air temperatures, **Then** the
   device's temperature delta, humidity-corrected delta, cloud cover and condition are those its
   own cloud logic computes for those readings and the current thresholds.
2. **Given** the visitor changes the differential control, **When** it is applied, **Then** the
   sky temperature moves to keep the air temperature fixed, and the sky and air controls show the
   new values.
3. **Given** rain at 0 mm/h, **When** the visitor sets 3 mm/h, **Then** the device's rain reading,
   its "raining" state, the verdict and the alerts follow its own rain logic.
4. **Given** the visitor sets illuminance, **When** the device has averaged it, **Then** SQM, NELM
   and Bortle are the device's own conversion of that light level.
5. **Given** any readings, **When** a sensor is set to "not responding", **Then** the device
   treats it as missing until it is cleared, with no time limit.
6. **Given** the rain sensor (or anemometer) is switched off in the device's settings, **When**
   the panel is open, **Then** its controls are unavailable, with the reason and a link to
   Settings → Sensors.

---

### User Story 2 - See what the device makes of it, and how long it takes (Priority: P1)

Next to the inputs, the panel shows what the device currently derives:
- SQM, NELM and Bortle
- cloud cover and condition
- dew point
- rain state
- the safety verdict and its reasons
- alerts armed or not

Where the device smooths or holds something, the panel shows it and the time left. Examples:

- "Sky brightness averages over 90 s - settled in 40 s"
- "Rain clear delay - 11 min 20 s until rain is cleared"
- "Safe delay - safe in 2 min"

**Why this priority**: The device smooths and delays on purpose, so a change can look ignored.
Showing the reading and the remaining time makes every control's effect visible and explains
waits.

**Independent Test**: Set rain to 2 mm/h, then to 0. The panel shows the device "raining" with the
rain clear delay counting down, and the verdict reason clears when it reaches zero.

**Acceptance Scenarios**:

1. **Given** an input has just changed, **When** the device's derived value hasn't caught up,
   **Then** the panel shows the input, the current derived value, and which averaging or delay it
   is waiting for, with the time remaining.
2. **Given** nothing is pending, **When** the panel is open, **Then** no wait indicator is shown.
3. **Given** the device's settings change, such as a longer averaging window, **When** the panel
   next updates, **Then** the shown waits use the new values.

---

### User Story 3 - Shortcuts worked out from the device's settings (Priority: P1)

Named shortcuts set several inputs at once, worked out from the device's **current** settings:

| Shortcut | What it sets |
|---|---|
| **Clear** | A differential safely below the clear-sky threshold, allowing for the humidity correction at the current humidity |
| **Overcast** | A differential just above the cloudy threshold |
| **Cloud just unsafe** | Cover just past the cloud-cover safety limit |
| **Rain** / **Rain stops** | Rain rate on or off |
| **Dark sky** | Illuminance for a chosen SQM |
| **Dew risk** | Humidity and temperature that put the dew point inside the alert margin |
| **Sensor fails** | One sensor fault |

Each shortcut says which settings it used, for example "sky -24 °C: your clear-sky threshold is
-13 °C". If the current settings make the target impossible, the shortcut says why and changes
nothing. One example is the cloud-cover rule being switched off.

A shortcut can optionally **ramp** an input over a set time, such as clouding over in 40 s. Once
applied, the inputs remain individually adjustable.

**Why this priority**: Visitors still want one-click outcomes, and docs links (`?scenario=`) and
the tour depend on them. Working them out from the live settings means they keep working however
the visitor has configured the device.

**Independent Test**: Set the clear-sky threshold to -30 °C and the cloudy threshold to -20 °C,
then press "Clear". The panel sets a differential below -30 °C (after humidity correction), says
so, and the device reads clear. Then press "Overcast": the differential is set above -20 °C and the
device reads overcast.

**Acceptance Scenarios**:

1. **Given** any valid cloud thresholds, **When** "Clear" or "Overcast" is pressed, **Then** the
   device reaches that condition once its averaging allows.
2. **Given** the cloud-cover safety rule is switched off, **When** "Cloud just unsafe" is pressed,
   **Then** the panel explains that the rule is off, with a link to it, and changes nothing.
3. **Given** a shortcut was applied, **When** the visitor changes one input, **Then** only that
   input changes.
4. **Given** a ramp is chosen, **When** the shortcut runs, **Then** the input moves smoothly from
   its current value to the target over the ramp time, running on the device clock.
5. **Given** the URL has `?scenario=<id>` (night, rain, cloud, clear, dawn, fail-light, fail-ir,
   fail-environment, fail-rain), **When** the demo loads, **Then** the matching shortcut applies.

---

### User Story 4 - Choose the date, time and place (Priority: P1)

The visitor sets the device's date, time and location, exactly or from presets.

Time presets:
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

The real sun position at that time and place drives the light input unless the visitor has set
illuminance directly. Sun & Moon, darkness and the night-only alerts all follow.

**Why this priority**: Dawn, darkness and the night rules depend on the real clock and place, and
visitors asked for them, for example 31 December at the North Pole.

**Independent Test**: Pick North Pole and 31 December. The device reports the sun below the horizon
all day, and Sun & Moon shows no sunrise. Pick "dawn": the panel explains there is no sunrise and
the clock does not jump.

**Acceptance Scenarios**:

1. **Given** any time, **When** "dawn" is picked, **Then** the clock moves to before sunrise at
   the device's location (default 45 min) and the light follows the real sun as the clock runs.
2. **Given** polar night or midnight sun, **When** a preset needs a sunrise or darkness that
   doesn't occur, **Then** the panel says why and the clock doesn't change.
3. **Given** an exact date and time is entered, **When** it is applied, **Then** the device clock
   runs from it at 1× or 10×.
4. **Given** a location preset is picked, **When** it is applied, **Then** the device's saved
   location and time zone change exactly as saving them in Settings would.
5. **Given** GPS is on, **When** a location is picked, **Then** the simulated GPS reports that
   place.

---

### User Story 5 - The panel works on a phone (Priority: P2)

On a phone, the Demo button and panel never cover the page's own controls. The open panel scrolls
within itself, and its close control stays reachable. The inputs are grouped by sensor and
collapsible, so the panel stays short.

**Why this priority**: The panel gains many controls, so it must stay usable on small screens.

**Independent Test**: At 375×667, open every settings tab and confirm every Save button can be
tapped. Open the panel and change an input in each group without horizontal scrolling.

**Acceptance Scenarios**:

1. **Given** a phone-sized screen, **When** a page has controls at the bottom, **Then** none sits
   under the Demo button.
2. **Given** the panel is open on a phone, **When** it is taller than the screen, **Then** it
   scrolls and can be closed with one tap.

---

### Edge Cases

- **Thresholds that make a shortcut impossible.** Examples: the cloud rule is off, or a sensor the
  target needs is off. The shortcut explains why and changes nothing (US3-2).
- **A differential requiring a sky temperature outside the sensor's range.** The MLX90614 reads
  roughly -70 °C to +380 °C, so the target is clamped to the range and the panel says so.
- **The visitor sets humidity above 100% or another physically impossible value.** The input is
  limited to the sensor's real range.
- **Rain sensor switched off while rain is set.** Rain stops being simulated and the controls
  become unavailable. When the sensor is switched back on, rain is 0 mm/h.
- **Illuminance set while the sun is up.** The visitor's value wins until they choose "follow the
  sun" again; the panel shows which source is active.
- **The device restarts** (demo restart, or GPS turned on). Inputs persist across the restart, as
  weather would.
- **10× speed.** Ramps, timed shortcuts and the shown waits run on the device clock.
- **An invalid or out-of-range date.** The date is rejected with a message and the clock doesn't
  change.
- **"Reset demo".** All inputs, the clock and the location return to defaults.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The panel's primary controls MUST be the raw readings each simulated sensor reports:
  - BME280: air temperature (°C), humidity (%) and pressure (hPa).
  - MLX90614: sky (object) temperature (°C) and sensor ambient temperature (°C).
  - TSL2591: illuminance (lux), with a "follow the sun" option.
  - RG-15: rain rate (mm/h) and lens fault.
  - Anemometer: wind speed and gust (m/s) and direction (°).
  - GPS: fix on or off, and position.
  - Every sensor: "not responding".
- **FR-002**: Each input MUST stay as set until changed. Changing one input MUST NOT change
  another, except as in FR-003.
- **FR-003**: The panel MUST offer a **sky minus ambient** differential control linked to the
  infrared readings. Changing the differential sets the sky temperature, keeping the ambient
  fixed. Changing either temperature updates the shown differential.
- **FR-004**: Inputs MUST be limited to each sensor's real measuring range. Out-of-range entries
  MUST be refused or clamped with a message.
- **FR-005**: Every derived value MUST come from the device's own logic, as in spec 016 FR-004:
  - cloud cover and condition
  - humidity-corrected delta
  - SQM, NELM and Bortle
  - dew point
  - rain state
  - the safety verdict and its reasons
  - alerts
  - Alpaca

  The panel MUST NOT compute or display any outcome the device didn't produce.
- **FR-006**: The panel MUST show the device's current derived values next to the inputs:
  - SQM, NELM and Bortle
  - cloud cover and condition
  - dew point
  - rain state
  - the verdict and its reasons
  - alerts armed
- **FR-007**: Where the device averages or holds a value, the panel MUST show which mechanism is
  pending and the time remaining. This covers the sky brightness averaging window, the rain clear
  delay, the safe delay and alert cooldowns. The values MUST come from the device's current
  settings.
- **FR-008**: Shortcuts MUST compute the inputs they set from the device's **current** settings:
  - **Clear** and **Overcast** use the clear-sky and cloudy thresholds plus the humidity
    correction at the current humidity.
  - **Cloud just unsafe** uses the cloud-cover limit.
  - **Dew risk** uses the dew-risk margin.
  - **Dark sky** uses a target SQM converted to illuminance with the device's own sky-brightness
    conversion.
  - **Rain** and **Rain stops** set the rain rate.
  - **Sensor fails** sets one sensor fault.

  Each shortcut MUST state the settings and values it used.
- **FR-009**: A shortcut whose target can't be reached with the current settings MUST say why,
  link to the relevant setting, and change nothing.
- **FR-010**: Any shortcut MAY ramp its inputs over a chosen time, running on the device clock.
  The default ramp for cloud changes is 40 s.
- **FR-011**: Outcome-only labels (for example, a "Cloud over" button that writes a fixed sky
  temperature) MUST NOT be primary controls. Shortcuts are secondary to the inputs, and
  everything they set is visible in the inputs afterwards.
- **FR-012**: `?scenario=<id>` links MUST keep working: night, rain, cloud, clear, dawn,
  fail-light, fail-ir, fail-environment and fail-rain each map to a shortcut, a time preset, or
  both.
- **FR-013**: The panel MUST let the visitor set the device's date and time exactly, and offer
  time presets:
  - now
  - tonight's darkest moment
  - dawn (default 45 min before sunrise)
  - dusk
  - midsummer midnight
  - midwinter midnight
  - 31 December 23:00

  The clock MUST run from the set time at 1× or 10×. Dawn and Night MUST move the clock; they MUST
  NOT fake the light.
- **FR-014**: The panel MUST offer location presets: London, La Palma, Atacama, Sydney, 75° N 1° W
  and the North Pole.
  - Applying one MUST change the device's saved location and time zone as Settings does.
  - The simulated GPS, when on, MUST report that place.
- **FR-015**: Where a time preset can't apply (no sunrise, no darkness), the panel MUST say why
  and leave the clock unchanged.
- **FR-016**: Controls for a sensor the device has switched off (rain, wind, GPS) MUST be
  unavailable, with the reason and a link to the setting.
- **FR-017**: The panel MUST show the device's date, time, time zone and location.
- **FR-018**: Inputs, the clock and the location MUST persist for the browser session, including
  across a refresh and an emulated restart, and MUST be cleared by "Reset demo" (spec 016 FR-008).
- **FR-019**: Nothing in this feature may make a network request beyond the demo's own files
  (spec 016 FR-006). Presets and time zone rules MUST be bundled.
- **FR-020**: On phone-sized screens, the Demo button and panel MUST NOT cover any page control.
  The panel MUST scroll within itself, with inputs grouped by sensor and collapsible.
- **FR-021**: Every control MUST be keyboard-usable and labelled for screen readers, with units in
  the accessible name.
- **FR-022**: The docs' Live Demo page MUST describe the inputs, shortcuts, waits and links.

### Key Entities

- **Sensor input**: one raw reading a simulated sensor reports, with its unit, real range, current
  value, and an optional ramp in progress.
- **Derived reading**: a value the device computed from the inputs (cloud cover, SQM, verdict and
  so on), shown as read from the device.
- **Pending wait**: an averaging window or hold the device applies, with its source setting and
  remaining time.
- **Shortcut**: a named set of input targets computed from the current settings. It records the
  settings it used, an optional ramp, an optional duration, and whether it is reachable.
- **Device clock** and **location preset**: as before. The clock is settable and runs at 1× or
  10×. A location preset has latitude, longitude, elevation and a POSIX time zone.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For any valid pair of cloud thresholds, "Clear" and "Overcast" make the device read
  that condition within its averaging time plus 10 s.
- **SC-002**: For every input, the device's derived value matches what its own logic gives for that
  input within one device tick after any pending wait ends. Checked by comparing with the native
  test build on the same inputs.
- **SC-003**: Every wait the device applies is shown with a remaining time accurate to within 2 s.
- **SC-004**: A shortcut that is unreachable with the current settings always explains why and
  changes no input. Covered by tests with the cloud rule off and with thresholds changed.
- **SC-005**: All existing `?scenario=` links still produce their visible outcome with default
  settings:
  - rain: unsafe, "Rain detected"
  - cloud: unsafe with a cloud reason
  - night: dark sky and "Dark now"
- **SC-006**: "Dawn" at London puts the device's sun between 6° and 12° below the horizon and
  rising. At the North Pole on 31 December, the panel explains there's no sunrise.
- **SC-007**: At 375×667, every Save button on every settings tab can be tapped with the Demo
  button present.
- **SC-008**: An automated run exercising every input, shortcut and preset records zero requests
  leaving the demo's origin.

## Assumptions

- The demo stays a browser-only, single-visitor emulation (spec 016). Inputs are per tab.
- **Shortcut margins.** "Clear" targets 3 °C below the clear-sky threshold, "Overcast" 1 °C above
  the cloudy threshold, and "Cloud just unsafe" 3% cover past the limit, all after the humidity
  correction. Each margin is chosen so the device's smoothing doesn't hover on the boundary.
- **Default inputs** are a typical clear night at the default location:
  - air 11 °C, humidity 62%, pressure 1013 hPa
  - sky -10 °C, IR ambient air +0.4 °C
  - light following the sun
  - rain 0, wind 3 m/s
- **Small natural variation.** Inputs keep a small variation so readings look alive. It is smaller
  than the shortcut margins and can be switched off ("hold steady") for exact testing.
- **Illuminance vs raw counts.** Illuminance is the light input; the raw TSL2591 channels are
  derived from it as today. Setting raw channels directly is out of scope.
- **Wind faults.** Only "not responding" is offered for the anemometer, matching the device's
  existing wind states.
- **Time zones** for presets are bundled as fixed POSIX rules, the device's own time zone format.
- **Panel layout** (grouping, sliders vs numbers, sheet vs floating on phones) is a planning
  decision. The requirements constrain only behaviour and reachability.
- **Tour (spec 018).** The tour drives these inputs and shortcuts; this spec doesn't define it.
