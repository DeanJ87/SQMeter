# Feature Specification: Bluetooth (BLE Build)

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the optional Bluetooth build: advertising, GATT readings and the paired phone alarm."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Wake me on my phone (Priority: P1)

The observer pairs a phone; events set to "wake me" ring it until acknowledged, even if the
phone is silenced, without needing internet.

**Why this priority**: The Bluetooth build exists for this.

**Independent Test**: Pair with the passkey, trigger a wake-me event, confirm the phone app is
indicated every 30 s until it writes the sequence to Ack.

**Acceptance Scenarios**:

1. **Given** a passkey and a paired phone, **When** a wake-me event fires, **Then** the alarm is
   indicated and repeats every 30 s until acknowledged, even if the condition clears.
2. **Given** alerts are paused (spec 021), **When** a wake-me event fires, **Then** the
   phone is not rung; with only "Send alerts" off (push channels), phones still ring.
3. **Given** the web UI, **When** the user acknowledges or unpairs all phones, **Then** it takes
   effect and is recorded.

---

### User Story 2 - Readings without WiFi (Priority: P3)

A phone or BLE proxy reads safety, rain and a sensor summary from advertisements and
characteristics.

**Acceptance Scenarios**:

1. **Given** Bluetooth is on, **When** a scanner listens, **Then** advertisements carry the safe and
   rain bits; GATT characteristics provide safety, rain, latest alert and summary JSON.

### Edge Cases

- Bluetooth slows WiFi (one radio): the UI warns.
- Standard build: Bluetooth settings show "needs the Bluetooth build".
- Turning Bluetooth on or changing the passkey needs a restart (prompted).
- "Wake me" without Bluetooth on or a paired phone is reported inactive with a fix (spec 020).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Bluetooth build MUST advertise safety/rain bits and serve read+notify
  characteristics for safety, rain, latest alert and a summary.
- **FR-002**: With a 6-digit passkey, the device MUST offer secured alarm/ack/heartbeat
  characteristics using LE Secure Connections with bonding.
- **FR-003**: "Wake me" events MUST ring paired phones, repeating every 30 s until acknowledged;
  pausing alerts (spec 021) MUST also stop new phone alarms.
- **FR-004**: The web UI MUST allow turning Bluetooth on, setting/generating the passkey,
  acknowledging the alarm and unpairing all phones.
- **FR-005**: The Bluetooth documentation MUST name settings as the UI does and describe when
  phones ring.

### Key Entities

- **Alarm**: sequence, reason flags, epoch, acknowledged sequence, bonded phones.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A paired phone is alerted within 5 s of a wake-me event.
- **SC-002**: Every settings path in the Bluetooth documentation exists in the UI.

## Assumptions

- A phone app implementing the alarm protocol is separate from this repository.
