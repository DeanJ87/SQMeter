# Feature Specification: Restart reason over MQTT

**Feature Branch**: `feat/mqtt-restart-reason`

**Created**: 2026-11-24

**Status**: Draft

## Context

An observatory SQMeter restarted four times on one night in November 2026, and twice more the next
day. The cause (a supply too small for the load) had to be worked out from gaps in the uptime,
because the restart reason is only shown by `/api/status` and the safety history, and the history
starts again at every boot. Nothing that records history (MQTT, Home Assistant, a time-series
database) saw why or when the device restarted.

SQMeter will publish why and when it last started over MQTT, and announce it to Home Assistant, so
restarts form a history that can be lined up against weather, roof runs and power work.

The ESP32 reports "brownout" both for a real supply dip and for a slow power-up, such as a large
supply's output rising after it is switched on. The published reason says so, so a switched-off
supply is not mistaken for a fault.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See why and when the device last restarted (Priority: P1)

The owner swaps the SQMeter's supply. In Home Assistant the device shows "Last restart reason:
brownout or power loss" and "Last boot: 20:10". The next morning both are unchanged, so the
device ran all night without a restart.

**Why this priority**: the restart reason and time are the whole feature.

**Independent Test**: restart the device with MQTT and Home Assistant discovery on; the device
topic and both entities show the reason and the boot time.

**Acceptance Scenarios**:

1. **Given** MQTT is on, **When** the device starts and connects to the broker, **Then** it publishes
   a retained device message with the restart reason (name and code) and the boot count.
2. **Given** the clock is not yet set at that moment, **When** the clock first becomes valid, **Then**
   the device message is published again with the boot time.
3. **Given** the broker connection drops and comes back, **When** it reconnects, **Then** the device
   message is published again unchanged.

---

### User Story 2 - A history of restarts (Priority: P2)

Home Assistant records each change of "Last boot" and "Last restart reason", so a week's restarts
can be counted and matched against events (a roof run, a power cut, a supply change).

**Why this priority**: the history is what diagnoses an intermittent supply fault, but it follows
from Story 1 without more device work.

**Independent Test**: restart the device twice; Home Assistant's history of "Last boot" shows two
changes at the right times.

**Acceptance Scenarios**:

1. **Given** Home Assistant discovery is on, **When** the device connects, **Then** "Last restart
   reason" and "Last boot" are announced as diagnostic entities of the SQMeter device.
2. **Given** discovery is switched off, **When** the settings are saved, **Then** both entities are
   removed with the others.

### Edge Cases

- The clock never becomes valid (no network time, no GPS): the boot time is published as null and
  "Last boot" shows unknown; the reason and count are still published.
- An unknown or new reset code: named "unknown", with the numeric code kept.
- Brownout: named `brownout`; documented and shown in Home Assistant as "brownout or power loss",
  since a slow power-up also reads as brownout.
- MQTT off: nothing is published; `/api/status` is unchanged.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: When MQTT is on, the device MUST publish a retained message on `<base>/device` after
  each broker connection, holding the restart reason as a name, the numeric reset code, the boot
  count and the boot time (UTC, ISO 8601) or null while the clock is not valid.
- **FR-002**: The device MUST publish the message again once, when the clock first becomes valid
  after boot, if the earlier message had no boot time.
- **FR-003**: Reason names MUST be fixed English tokens: `poweron`, `external`, `software`, `panic`,
  `watchdog` (interrupt, task and other watchdogs), `deepsleep`, `brownout`, `sdio`, `usb`, `jtag`,
  `efuse`, `powerglitch`, `cpulockup`, `unknown`.
- **FR-004**: With Home Assistant discovery on, the device MUST announce a "Last restart reason"
  sensor and a "Last boot" timestamp sensor as diagnostic entities read from `<base>/device`, and
  remove them when discovery is switched off, like its other entities.
- **FR-005**: The device topic MUST NOT depend on the publish-group switches (it is one small
  retained message per connection).
- **FR-006**: The MQTT user guide MUST document the topic, its fields, the names and the brownout
  caveat.

### Key Entities

- **Device message**: reason name, reason code, boot count, boot time (or null). Retained on
  `<base>/device`; replaced at each boot.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After any restart with MQTT on, the restart reason is visible in Home Assistant
  within 1 minute of the device reconnecting.
- **SC-002**: The boot time shown is within 5 seconds of the actual start, once the clock is set.
- **SC-003**: Every restart over a week appears as a separate change in Home Assistant's history
  of "Last boot".

## Assumptions

- The boot count is the existing counter; it starts again after a full power loss.
- No web UI change: the dashboard already shows restarts in the safety history.
- The time-series database takes the new entities through Home Assistant's existing recorder; no
  separate MQTT-to-database change is part of this feature.
- English-only payload values, as for every other topic (spec 023 FR-015).
