# Feature Specification: Rain Sensor (Hydreon RG-15)

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Know when it's raining (Priority: P1)

The observatory owner fits an RG-15 and the device reports rain within one poll, keeping the
"raining" state for a configurable time after the last drop.

**Why this priority**: Rain is the one condition that damages equipment; everything else can wait.

**Independent Test**: Wet the sensor; within one poll interval the device reports rain, and it
stays "raining" for the clear delay after drying.

**Acceptance Scenarios**:

1. **Given** the RG-15 is enabled and online, **When** it reports a non-zero intensity, **Then**
   `isRaining` and the latched `raining` state become true within one poll.
2. **Given** rain has stopped, **When** the clear delay (default 15 min) passes with no rain,
   **Then** `raining` clears.
3. **Given** the sensor is offline, stale or reports a lens fault, **When** status is read,
   **Then** that state is reported so safety can treat it as a fault.

---

### User Story 2 - See how much rain (Priority: P2)

The owner sees intensity, event accumulation and a daily total in the configured units.

**Why this priority**: Useful context, not safety-critical.

**Independent Test**: Compare readings with the RG-15's own output; the daily total resets at
the configured time when enabled.

**Acceptance Scenarios**:

1. **Given** readings, **When** shown on the dashboard, **Then** intensity, event and daily
   accumulation are shown with the units the sensor is configured for.
2. **Given** a daily reset time, **When** it passes, **Then** the total is reset once.

---

### User Story 3 - Bring up and service the sensor (Priority: P3)

The builder tests communication, resets the total and reboots the sensor from the web UI, and
sees UART diagnostics when wiring is wrong.

**Why this priority**: Bring-up and troubleshooting.

**Independent Test**: Run the communication test with the sensor unplugged and see a clear
timeout; plug it in and see a successful response.

**Acceptance Scenarios**:

1. **Given** the Sensors settings, **When** the user runs the communication test, **Then** the
   raw command, response, acknowledgement and any error are shown.
2. **Given** the System page, **When** the user resets the total or reboots the sensor, **Then**
   the command result is shown; both require the password when protection is on.

### Edge Cases

- Sensor disabled: no rain data appears anywhere (API, dashboard, Alpaca returns
  NotImplemented for rain rate).
- Garbled responses: counted as parse errors, not readings.
- Imperial units: Alpaca still reports rain rate in mm/h.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST poll the RG-15 at a configurable interval and report intensity,
  accumulation since last read, event accumulation, total accumulation, lens-fault and
  emitter-saturation flags, online/stale state and age.
- **FR-002**: The device MUST keep a latched `raining` state that stays true for
  `rainClearDelayMs` (default 900000 ms) after the last reading with non-zero intensity or
  non-zero accumulation (either means rain was seen).
- **FR-003**: The device MUST support resolution (high/low/DIP switch) and units
  (metric/imperial/DIP switch) settings. *Superseded in part by 013*: the API, MQTT and Alpaca
  always report mm and mm/h (a sensor set to inches is converted); the web UI shows the
  configured units.
- **FR-004**: The device MUST optionally reset the total accumulation once a day at a
  configured local time, and on demand. The daily reset fires on the first check at or after
  that time, once per day, and survives restarts and clock changes (a restart or DST jump over
  the reset minute neither skips nor repeats it).
- **FR-005**: The device MUST expose UART diagnostics (pins, port, last command/response,
  timeouts, parse errors, successful reads) and offer test, reset-total and reboot actions,
  each requiring the password when protection is on.
- **FR-006**: When the sensor is disabled, no rain object MUST appear in the sensor payload and
  no rain card on the dashboard.
- **FR-007**: The latched rain state and sensor health MUST feed the safety verdict and the
  rain alerts; the rain rate MUST feed Alpaca in mm/h.
- **FR-008**: Each reading MUST be reported under one field name; the documented payload MUST
  list every field the device sends.
- **FR-009**: The hardware documentation MUST describe wiring, DIP switches, configuration,
  payload and troubleshooting consistently with the device.

### Key Entities

- **Rain reading**: intensity, accumulations, flags, latched state, timestamp.
- **RG-15 diagnostics**: UART configuration and counters, last command/response, timing.
- **Rain settings**: enabled, pins, baud, resolution, units, poll interval, clear delay,
  daily reset.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Rain is reported within one poll interval (default 5 s) of the sensor detecting it.
- **SC-002**: The latched state clears within one poll after the clear delay, never earlier.
- **SC-003**: 100% of fields sent by the device are documented, and every documented field is sent.
- **SC-004**: A builder can diagnose a wiring fault from the web UI without a serial console.

## Assumptions

- Communication is polling only; the `mode` setting is kept for compatibility.
- The RG-15's own event timer (`EventAcc`) and the device's local event accumulation are both
  useful and are reported separately.
