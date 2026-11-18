# Feature Specification: Safety holds

**Feature Branch**: `feat/safety-holds`

**Created**: 2026-11-18

**Status**: Implemented

## Context

The SafetyMonitor decides from SQMeter's own sensors. Other systems in an observatory know things
SQMeter can't measure: Home Assistant sees smoke from a neighbour's fire on its PM2.5 sensor, a
roll-off-roof controller knows the roof didn't open fully. Until now the only way to force unsafe
was the **Force unsafe** setting, one switch with no reason and no owner, which an automation
clearing it could use to turn off someone else's reason.

A **hold** is a named event that keeps the SafetyMonitor unsafe until the system that set it
clears that event. Clearing a hold never forces safe: the other rules (cloud, rain, ...) still
decide. The Force unsafe setting becomes the built-in hold `manual`.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Another system holds the observatory unsafe (Priority: P1)

Home Assistant sees PM2.5 spike and sets hold `smoke` with the reason "Smoke: neighbour's fire,
PM2.5 85 µg/m³". The SafetyMonitor is unsafe at once (no unsafe delay), Alpaca clients see
`IsSafe = false`, the dashboard shows the reason with who set it and when, and the unsafe alert
carries the reason. When the fire subsides Home Assistant clears `smoke`; if the sky is cloudy it
stays unsafe for cloud.

**Independent Test**: `pio test -e native -f test_safety_holds`; in the demo, `POST
/api/safety/hold` then `DELETE /api/safety/hold/smoke` while the demo sky is cloudy.

**Acceptance Scenarios**:

1. **Given** a safe verdict, **When** hold `smoke` is set, **Then** the verdict is unsafe on the
   next evaluation, its reason is in `reasons`, `reasonFlags` has the external-hold bit and
   `holds` lists it.
2. **Given** holds `smoke` and `roof`, **When** `smoke` is cleared, **Then** `roof` still holds
   the verdict unsafe.
3. **Given** hold `smoke` and cloud over the limit, **When** `smoke` is cleared, **Then** the
   verdict stays unsafe for cloud and the history says "Hold cleared: Smoke (still unsafe: cloud)".
4. **Given** no hold `smoke`, **When** it is cleared, **Then** nothing changes and the answer says
   there was no such hold (HTTP 404, MQTT ignored and logged).

---

### User Story 2 - A hold that lifts itself (Priority: P2)

A hold set with `expiresMin` lifts at that time, also after a restart, so a forgotten hold doesn't
keep the roof shut for a week.

**Independent Test**: native tests drive the clock past the expiry, with and without a valid wall
clock, across a simulated restart.

---

### User Story 3 - Force unsafe is a hold like any other (Priority: P2)

The Force unsafe toggle sets and clears the hold `manual`; the dashboard shows it like any other
hold, with a Clear link that switches the toggle off. Old settings with Force unsafe on keep it on.

---

### Edge Cases

- Setting an existing id again replaces its reason, source and expiry and keeps its `since`.
- More than 8 holds, a bad id, an empty or over-long reason, or an `expiresMin` outside 1..10080:
  refused with 400 and nothing changes.
- Restart without a valid clock: a hold with an expiry lifts no earlier than its full duration
  after boot, or at its wall-clock time once the clock is set, whichever comes first. It never
  lifts early.
- Holds survive restarts and power cuts (NVS), written only when a hold is set or cleared.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A hold has an id (`[a-z0-9_-]{1,24}`), a reason (1-96 characters, no control
  characters), a source (up to 32 characters; default `API` over HTTP, `MQTT` over MQTT), when it
  was set, and optionally when it lifts. At most 8 holds exist at once.
- **FR-002**: The verdict is unsafe while any hold exists. External holds add their reason to
  `reasons` and set `reasonFlags` bit 17 (`UnsafeExternalHold`); the `manual` hold sets bit 0 with
  the reason "Manual override forces unsafe", as Force unsafe always did. Both bits skip the
  unsafe delay; clearing a hold leaves the normal safe delay in effect.
- **FR-003**: `POST /api/safety/hold` `{"id","reason","source"?,"expiresMin"?}` sets or updates a
  hold and answers 200 with it; `?source=` is used when the body has none. `DELETE
  /api/safety/hold/<id>` and `POST /api/safety/hold/<id>/clear` clear one hold: 200
  `{"cleared":true}`, or 404 `{"cleared":false,"reason":"no such hold"}` with nothing changed.
  Both need the password when protection is on, like every route that changes something.
- **FR-004**: MQTT `<base>/safety/hold/set` with `{"id","reason","source"?,"expiresMin"?}` sets a
  hold; `{"id","clear":true}` clears it. A bad payload or an unknown id is ignored and logged.
- **FR-005**: `GET /api/safety`, the readings' `safety` and MQTT `<base>/safety` list `holds`:
  `{id, reason, source, sinceEpoch | sinceAgeMs, expiresEpoch?, expiresInMs?}`.
- **FR-006**: `expiresMin` (1-10080) lifts the hold automatically. With a valid clock the lift time
  is stored as wall-clock time; after a restart the hold also lifts once its full duration has
  passed since boot, so it lifts even if the clock is never set, and never before its time.
- **FR-007**: Holds are stored in NVS on every set and clear (including an expiry) and restored at
  boot; nothing is written periodically.
- **FR-008**: `alpaca.manualOverrideUnsafe` still reads and writes: true sets (or keeps) the hold
  `manual`, false clears it, and setting or clearing `manual` through the API saves the setting.
  At boot the setting decides whether `manual` exists, so old settings migrate unchanged.
- **FR-009**: The safety history records a hold set while already unsafe (`holdSet`) and every
  hold cleared or lifted (`holdCleared`) with its id, a short label (the reason up to its first
  colon) and the reasons it is still unsafe for. Verdict changes list the labels of the external
  holds. A hold set while safe is recorded by the verdict change it causes.
- **FR-010**: Alerts whose text lists the unsafe reasons include the hold reasons (they are in
  `reasons`).
- **FR-011**: The Safety Monitor card lists each hold under the other reasons: its reason, a muted
  line `Hold "<id>" · <source> · since HH:MM[ · lifts HH:MM]` and a Clear link. Settings → Safety
  has one muted line under Force unsafe with an InfoTip giving the HTTP and MQTT calls.
- **FR-012**: The hold logic lives in `lib/AlpacaLogic` (`SafetyHolds`), is unit tested natively and
  runs in the demo's device core, which answers the same routes.

### Key Entities

- **Hold**: id, reason, source, since (wall clock or device clock), duration, lift time.

## Success Criteria *(mandatory)*

- **SC-001**: A hold set by HTTP or MQTT makes Alpaca `IsSafe` false within one evaluation (1 s).
- **SC-002**: Clearing one hold never changes another hold or any other rule's verdict.
- **SC-003**: Holds and their expiry survive a restart; flash is written only on set and clear.

## Assumptions

- Holds use the same password protection as the other routes that change something (HTTP Basic
  auth when protection is on); there is no separate token.
- The 8-hold cap and the length limits keep the stored list under 2 KB and the safety document
  within its fixed JSON capacity.
