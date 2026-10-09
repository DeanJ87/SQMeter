# Feature Specification: Safety Monitor

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the safety verdict: rules, fail-safe behaviour, reasons, safe delay, history and where the verdict is published."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A verdict I can trust with the roof (Priority: P1)

Imaging software asks "is it safe?" and gets a fail-safe answer: rain, wind, missing or stale data,
and sensor faults all make it unsafe.

**Why this priority**: The verdict protects equipment; a wrong "safe" is costly.

**Independent Test**: Simulate each rule and each failure mode (stale data, faulted sensors, rain
while other data is stale) and check the verdict and reasons.

**Acceptance Scenarios**:

1. **Given** any enabled rule fails, **When** the verdict is evaluated (every second), **Then** it
   is unsafe and lists every failing rule with the measured value and the limit.
2. **Given** the sky data is stale, **When** it rains, **Then** the verdict is still unsafe for rain.
3. **Given** a limit is enabled but can't be measured (no anemometer, no humidity sensor), **When**
   evaluated, **Then** the verdict is unsafe and says why.
4. **Given** no successful read since boot, or data older than the stale threshold, **When**
   evaluated, **Then** the verdict is unsafe.

---

### User Story 2 - Don't reopen on a gap in the clouds (Priority: P2)

A safe delay holds "safe" back until conditions have been safe for a set time; unsafe is immediate.

**Why this priority**: Prevents roof cycling.

**Independent Test**: Flip conditions safe for less than the delay and confirm the verdict stays
unsafe, then wait out the delay.

**Acceptance Scenarios**:

1. **Given** a safe delay, **When** conditions become safe, **Then** "safe" is reported only after
   they've stayed safe for the delay, and the dashboard shows "Safe in Ns" meanwhile.
2. **Given** a restart, **When** the device boots, **Then** the delay runs from boot.

---

### User Story 3 - See why, and what happened (Priority: P2)

The user sees the verdict, the reasons, how long it's been that way, and a history of changes,
restarts and alerts sent that survives restarts.

**Why this priority**: Explains "why is it unsafe" and "why didn't I get an alert".

**Acceptance Scenarios**:

1. **Given** the dashboard, **When** the Safety card is shown, **Then** it shows the verdict, the
   reasons, "unsafe/safe for N" and a History list on demand.
2. **Given** a software restart or crash, **When** History is opened, **Then** earlier entries and
   the restart reason are still there.

---

### User Story 4 - Use the verdict anywhere (Priority: P3)

The verdict is available to Alpaca clients, the REST API (including a plain 1/0), the live stream
and MQTT.

**Acceptance Scenarios**:

1. **Given** any consumer, **When** it reads the verdict, **Then** every channel reports the same
   value at the same time.

### Edge Cases

- A faulted light or IR sensor must not produce a misleading threshold reason (e.g. 100% cloud).
- Disabled rules never contribute.
- Power cut: history is lost (RTC memory), and that's acceptable.
- Rain rules with the rain sensor switched off are not in effect and are listed as such
  (*superseded in part by spec 020, D-15*); wind limits without an anemometer stay fail-safe
  unsafe (D-16).
- "Stale data" means the light or IR sensor's last *successful* read is older than the stale
  limit - a sensor that keeps reporting OK without fresh readings is stale, not just a stalled
  read loop. A sensor that stopped answering is a sensor fault.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST evaluate the verdict every second from: manual override, rain (latched),
  rain sensor health, wind and gust limits, no data yet, stale data, light/IR sensor fault, cloud
  cover, SQM minimum, humidity maximum, dew-point margin and humidity-sensor fault.
- **FR-002**: Rain and wind rules MUST be evaluated regardless of other data freshness; threshold
  rules MUST only use fresh data from healthy sensors.
- **FR-003**: Every rule MUST have its own enable switch, with defaults as documented. Rules whose
  sensor is switched off follow spec 020 (rain rules not in effect; wind limits fail-safe).
- **FR-004**: Each unsafe reason MUST include the measured value and the limit.
- **FR-005**: A safe delay (0–3600 s) MUST hold back "safe", run from boot, and report seconds
  remaining; unsafe MUST be immediate.
- **FR-006**: The verdict MUST be published to Alpaca `IsSafe`, `GET /api/safety`, `GET /api/safe`
  (1/0), the live stream and MQTT (`<topic>/safe`, `<topic>/safety`, `safe` in readings).
- **FR-007**: The device MUST keep the last 32 safety changes, restarts (with reason) and
  safe/unsafe alerts sent, surviving software restarts and crashes.
- **FR-008**: The Settings must offer all rules on one Safety tab; the documentation MUST name
  them as the UI does.

### Key Entities

- **Safety thresholds**: per-rule enable and limit, stale threshold, safe delay, override.
- **Safety status**: verdict, raw verdict, reasons, reason flags, seconds until safe, ages.
- **History entry**: kind (boot / change / alert / alerts on-off), time, verdict, flags, reset reason.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of rule and failure-mode combinations produce the documented verdict.
- **SC-002**: All publication channels agree on the verdict within one evaluation (1 s).
- **SC-003**: Every setting named in the safety documentation exists in the UI under that name.

## Assumptions

- The reported verdict (after the safe delay) is what clients and alerts use; the raw verdict is
  diagnostic.
