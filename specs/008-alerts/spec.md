# Feature Specification: Alerts

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill device-sent alerts: events, per-event levels and sounds, custom wording, stacking, darkness limits, restarts, channels, tests, the recent list and switching alerts off when not imaging."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Be told when it matters, at the right volume (Priority: P1)

The observer chooses, per event, whether it's off, quiet, normal, urgent or "wake me", and with
which Pushover sound — e.g. clouds rolling in at 3 am wakes them, skies clearing doesn't.

**Why this priority**: The maintainer's core requirement: the right alerts, at the right level.

**Independent Test**: Set different levels per event, trigger each, and check channel priority,
sound and Bluetooth alarm behaviour.

**Acceptance Scenarios**:

1. **Given** per-event levels, **When** an event fires, **Then** Pushover priority (−1..2, wake =
   repeating emergency), ntfy priority and the Bluetooth alarm follow the level; the event's sound
   (or the channel default) is used.
2. **Given** several events at once (rain + unsafe), **When** they fire, **Then** one notification
   is sent at the loudest level, listing all events.
3. **Given** an unsafe alert, **When** sent, **Then** it lists every reason with value and limit.

---

### User Story 2 - Only when it's relevant (Priority: P1)

Sky and safety changes are only announced while it's dark; restarts don't produce phantom
"safe" alerts; alerts can be switched off while not imaging.

**Why this priority**: False or irrelevant alarms at night erode trust (dawn unsafe alarm,
restart "safe").

**Acceptance Scenarios**:

1. **Given** "only when it's dark", **When** the sky or verdict changes in daylight, **Then**
   nothing is sent; at nightfall the verdict is compared with what was last announced.
2. **Given** a restart, **When** the device starts up, **Then** start-up states aren't announced,
   and after the grace period the verdict is compared with the last safe/unsafe alert sent.
3. **Given** alerts are switched off (UI, REST, MQTT, or automatically when N.I.N.A.
   disconnects), **When** events occur, **Then** nothing is sent; switching on sends one quiet
   "Alerts on" with the current verdict.

---

### User Story 3 - My own wording (Priority: P2)

The observer writes per-event titles and messages with `{variables}` and tests them before saving.

**Acceptance Scenarios**:

1. **Given** a template, **When** an event fires, **Then** variables are filled from current
   readings, limits and event data; unknown names stay visible.
2. **Given** the editor, **When** the user inserts a variable, **Then** every variable the device
   supports is offered, with the defaults shown as they will actually be sent.
3. **Given** unsaved changes, **When** Test is pressed, **Then** a sample with that level, sound
   and wording is sent to every enabled channel and each channel's result is shown.

---

### User Story 4 - Channels and history (Priority: P2)

Pushover, ntfy, webhook and MQTT can each be enabled and tested; recent alerts with per-channel
delivery results appear under the bell and can be cleared.

**Acceptance Scenarios**:

1. **Given** a channel test, **When** sent, **Then** the real delivery result (sent/failed with
   reason/skipped) is reported, not just "queued".
2. **Given** the bell, **When** opened, **Then** the last 20 alerts since boot are listed with
   per-channel status, and Clear empties the list.

### Edge Cases

- Cooldown: a change held back is sent when the cooldown ends if it still differs.
- Sensor blips shorter than 30 s aren't announced.
- No channel enabled: "wake me" tests ring paired phones only.
- One HTTPS session at a time; sends retry once on connection failure.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Events: unsafe, safe, rain started/stopped, sensor fault (incl. lens) / recovered,
  dew risk, skies clear up, skies cloud over; each with level 0–4, sound, optional title/message.
- **FR-002**: Levels MUST map to channel priorities as documented, and "wake me" MUST also ring
  paired Bluetooth phones.
- **FR-003**: Simultaneous events MUST be stacked into one notification at the loudest level.
- **FR-004**: Sky and safety alerts MUST be limitable to darkness (default on), sharing one
  darkness level; restarts MUST compare with the last announced verdict.
- **FR-005**: Alerts MUST be switchable off/on via UI, REST, MQTT and optionally N.I.N.A.
  connect/disconnect, persistently.
- **FR-006**: Templates MUST support every variable the device provides; the UI MUST offer exactly
  that set and show the device's actual default wording.
- **FR-007**: Channel and event tests MUST report each channel's real result.
- **FR-008**: The recent-alerts list (20, since boot) MUST show per-channel results and be clearable.
- **FR-009**: The alerts documentation MUST use the UI's labels and describe behaviour as implemented.

### Key Entities

- **Event setting**: level, sound, title, message.
- **Alert**: type, level, title, message, sound, variables, stacked types.
- **Delivery record**: id, time, alert, per-channel status and detail.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 0 alerts sent for start-up states, daylight changes (when limited) or while switched off.
- **SC-002**: Test results reflect real delivery for 100% of channels.
- **SC-003**: Every variable the device supports is offered in the editor and documented.
- **SC-004**: Every UI label quoted in the alerts documentation exists in the UI.

## Assumptions

- Pushover's built-in sound list is fixed in the UI; custom sounds already saved stay selectable.
