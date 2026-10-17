# Feature Specification: OLED display

**Feature Branch**: `spec/031-oled-display`

**Created**: 2026-10-17

**Status**: Draft

## Context

SQMeter has no screen: every reading is read through the web UI, an app or an integration. That works
for a fixed install, but the travel board (SQMeter-Hardware, battery powered, no field wiring) is used
in a field with no network, and even at home a glance at the device is quicker than a phone. The Rev A
boards have a dedicated JST-XH port for a small monochrome OLED (SSD1306-class, 128 × 64, I2C address
0x3C) on the main I2C bus, and any board can use its spare I2C port for one.

A screen at an observing site is also a light source. The display must never cost the observer their
dark adaptation unless they ask for light, so it is off, dim or minimal by default after dark.

Out of scope: colour or touch displays; displays on other interfaces (SPI, parallel); drawing charts or
history on the device.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read the sky at a glance (Priority: P1)

An observer in the field with no network switches the travel unit on and reads the sky quality straight
off the device.

**Why this priority**: it is the reason to fit a screen.

**Independent Test**: fit an OLED, enable the display, power the device with no network, and confirm the
sky quality page appears with the authoritative reading and refreshes with each new reading.

**Acceptance Scenarios**:

1. **Given** an OLED fitted and the display enabled, **When** the device boots, **Then** the first page
   shows the authoritative sky reading in large numerals with its name ("SQM" or "SQM-L", spec 030) and
   unit, Bortle class and NELM, and, when a second sky sensor reports, the other reading in smaller type.
2. **Given** the display showing a reading, **When** the sky sensor faults or its data goes stale,
   **Then** the page shows a clear no-data marker in place of the number, never the last good value.
3. **Given** no OLED is fitted, **When** the device runs, **Then** nothing changes: no errors in normal
   operation, the display reports as not detected, and the display settings say why they can't apply.

---

### User Story 2 - Pages for everything else (Priority: P2)

The observer wants temperature, humidity and dew point, cloud (sky temperature), GPS fix, battery and the
safety verdict without reaching for a phone, and to choose which of those appear and in what order.

**Why this priority**: the display is far more useful with more than one number, but the first page
alone is already a working feature.

**Independent Test**: enable three pages, set their order and a 5-second cycle, and confirm the screen
shows exactly those pages in that order, each for 5 seconds.

**Acceptance Scenarios**:

1. **Given** the page list, **When** the owner picks pages and their order, **Then** the display cycles
   through only those pages, in that order, at the configured interval.
2. **Given** a page whose data source is absent (no GPS, no battery, sensor disabled), **When** the
   display cycles, **Then** that page is skipped rather than shown empty.
3. **Given** the network page, **When** it shows, **Then** it gives the device's name, IP address (or
   access-point name and address during WiFi setup) and connection state, so a user can find the web UI.

---

### User Story 3 - Protect dark adaptation (Priority: P1)

At night the observer wants the screen dark unless they ask, and dim and minimal when it is on.

**Why this priority**: a bright screen at a dark site is a defect, not a feature.

**Independent Test**: after astronomical dusk at the configured location, confirm the display turns off
after the configured timeout and, when woken, shows at minimum brightness with the night layout.

**Acceptance Scenarios**:

1. **Given** night (from the device's location and time, as the dashboard already uses), **When** the
   display has been on for the timeout, **Then** it turns fully off.
2. **Given** night mode, **When** the display is woken, **Then** it uses minimum brightness and a minimal
   layout (one large reading, no inverse blocks or borders) and returns to off after the timeout.
3. **Given** the owner opts to keep the display on all night, **When** night falls, **Then** it stays on
   in night mode, and this choice is shown in Settings as using light at night.

---

### User Story 4 - Wake without a button (Priority: P3)

The display has no button by default. The observer wakes it from the web UI, by power-cycling, or with an
optional button wired to a spare GPIO.

**Why this priority**: auto-off needs a way back on, but the default wake on boot covers the basic case.

**Independent Test**: with the display off, press Wake in the web UI and confirm it turns on for the
timeout; with a button configured on an AUX pin, confirm a press wakes it and a second press advances the
page.

**Acceptance Scenarios**:

1. **Given** the display off, **When** the owner presses Wake in the web UI or calls the wake endpoint,
   **Then** the display turns on for the timeout.
2. **Given** a wake button configured on a free GPIO, **When** it is pressed, **Then** the display wakes;
   pressing again while on shows the next page.

### Edge Cases

- The OLED is plugged in or removed while running: detection updates without a restart; the display
  resumes when it returns.
- Long values (IP addresses, names) don't fit: they are shortened with an ellipsis, never wrapped off
  screen.
- Burn-in: static content shifts by a pixel or two at each page change, and the default auto-off limits
  how long any page stays lit.
- The device restarts at night: wake-on-boot shows the display for the timeout in night mode, then off.
- No location or time set: night can't be computed, so the display uses the timeout and the owner's
  night-mode choice applies all the time until location and time are known.
- Two devices on the same I2C address (another 0x3C device on the bus): the display is reported as
  detected when something at 0x3C acknowledges and answers a one-byte read (research D3: SSD1306
  status bits are no reliable identity across clones), so another device there that answers reads
  is taken for the display; the clash is documented.

## Clarifications

### Session 2026-10-17 (owner)

- Q: What language does the screen use? → A: English short labels and numerals, always, whatever the
  device language. The web UI's display settings are translated as usual.

## Requirements *(mandatory)*

### Functional Requirements

**Display and pages**

- **FR-001**: The device MUST detect an SSD1306-class 128 × 64 monochrome OLED at I2C address 0x3C on the
  main I2C bus at start-up and when the display is enabled, and report detected / not detected.
- **FR-002**: The display MUST be disabled by default and enabled in Settings. A disabled display is not
  driven and stays dark.
- **FR-003**: The display MUST offer these pages: **Sky** (authoritative sky reading, its name, Bortle,
  NELM, the second sky reading when present), **Environment** (temperature, humidity, dew point),
  **Cloud** (sky temperature and cloud state), **Safety** (verdict and the first reason), **GPS** (fix,
  satellites, position), **Battery** (state of charge, charging state; spec 032), **Network** (name, IP or
  setup access point, connection state).
- **FR-004**: The owner MUST be able to choose which pages are shown and their order, and the cycle
  interval (2–60 s, default 5 s). With one page selected the page doesn't cycle. Sky is the default and
  only page initially.
- **FR-005**: Pages whose data source is disabled or absent MUST be skipped. Values whose data is missing,
  faulted or stale MUST show a no-data marker, never a stale value (matching constitution I's treatment of
  stale data).
- **FR-006**: The display MUST refresh a shown value within one sensor read interval of the new reading.

**Light at night**

- **FR-007**: Brightness MUST be settable (low / medium / high, default medium by day).
- **FR-008**: The display MUST turn off after a timeout (10 s–30 min, or never; default 60 s). It wakes
  for the timeout on boot (on by default, switchable), on a wake request, or on a wake-button press.
- **FR-009**: At night (sun below the twilight limit at the configured location, as the device computes
  today), the display MUST use night mode: minimum brightness, minimal layout, and the timeout applies
  even when the timeout is set to never, unless the owner explicitly enables "stay on at night".
- **FR-010**: The screen orientation MUST be selectable (normal or rotated 180°) to suit the enclosure.

**Text**

- **FR-011**: On-screen text MUST be English short labels and numerals that fit 128 × 64 (for example
  "SQM", "NELM", "Temp", "Dew", "Safe"), in every device language; no translated text or extra fonts on
  the device. Units follow the device's unit settings. The docs list every on-screen label and its
  meaning.

**Wake and control**

- **FR-012**: The owner MUST be able to configure an optional wake button on a free GPIO (the AUX pins on
  the Rev A boards). Pin choice is validated by the existing pin-conflict checks. A press wakes the
  display; a press while on shows the next page.
- **FR-013**: The REST API MUST offer authenticated actions to wake the display and to blank it (constitution
  VI: changes require the password when one is set), and report display status in `/api/status`
  (`display.detected`, `display.on`, `display.page`, `display.nightMode`).
- **FR-014**: Display settings MUST be part of the configuration (enable, pages and order, cycle interval,
  brightness, timeout, wake on boot, stay on at night, orientation, wake button pin), exposed through the
  existing settings API, the OpenAPI description and the demo device.

**UI, budgets, docs, languages**

- **FR-015**: Settings MUST gain a Display card with the options above and a Wake button. Options that
  can't apply (no display detected; night options without a location) MUST be shown disabled with a
  reason and link through the settings dependencies catalogue (spec 020).
- **FR-016**: The feature MUST stay within the resource budgets (constitution IV), and a device without a
  display MUST not pay noticeable runtime cost for it.
- **FR-017**: Configuration saved before this feature MUST load with the display off.
- **FR-018**: The documentation MUST cover the display in the same change: a user-guide page (pages,
  night behaviour, wake), wiring and the port on the hardware pages, configuration, REST and settings
  dependencies references.
- **FR-019**: Every new web UI string MUST be added to the English catalogue with its context note and
  translated into all 13 other UI languages through the existing translation workflow. On-screen OLED
  labels are not translated (FR-011).

### Key Entities

- **Display**: detected state, on/off, current page, night mode, and its settings.
- **Page**: one of the fixed page types, with its data sources and whether it currently has data.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An observer can read the sky quality off the device within 5 seconds of switching it on,
  with no network.
- **SC-002**: With default settings, the display emits no light after astronomical dusk except for at most
  the timeout after a boot or wake request.
- **SC-003**: A user can find the device's web address from the Network page alone on first use.
- **SC-004**: Devices without a display show no change in behaviour, API output (beyond the new status and
  settings fields) or responsiveness.
- **SC-005**: Every new string appears in all 14 UI languages and the docs build passes.

## Assumptions

- Monochrome OLEDs can't show red, so night protection comes from off, minimum brightness and a minimal
  layout rather than colour.
- The device already knows night from location and time (used by the dashboard); without them, the
  timeout governs.
- One display at most; it shares the main I2C bus with the sky, environment and infrared sensors.
- The fuel-gauge and charger data for the Battery page come from spec 032; the second sky reading from
  spec 030. Each page appears only when its feature is present.

## Dependencies

- Spec 020 (settings dependencies), 023 (i18n), 028 (OpenAPI), 030 (second sky sensor), 032 (battery).
- Board profiles for the ESP32-S3 boards (separate work) define the display port and AUX pins.
