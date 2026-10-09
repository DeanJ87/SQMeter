# Feature Specification: Alert schedule wording and "imaging app lost" alerts

**Feature Branch**: `spec/021-alert-schedule`

**Created**: 2026-10-08

**Status**: Implemented (PR #95; spec PR #84) - converged

**Input**: User description: "'On while N.I.N.A. is connected' doesn't describe what it does - it's really 'off when the SafetyMonitor Alpaca client disconnects', and that's bad wording too. 'Alerts on now?' implies switching it on bypasses something - confusing even to a native English speaker. Add the inverse: notify me if N.I.N.A. (or any Alpaca client) disconnects or stops polling for a set time, so I know the imaging software has dropped its connection to the safety monitor."

## Context

Today the Alerts settings (spec 008, FR-005) have a group titled **"When you're not imaging"** with
two toggles:

| Today's control | What it actually does |
|---|---|
| **Alerts on now** | A live flag ("armed"). Off: nothing is sent and phones don't ring. Also switched by MQTT `<topic>/alerts/armed/set` and `POST /api/alerts/arm` / `disarm`. |
| **On while N.I.N.A. is connected** (`alerts.armWithAlpaca`) | When any Alpaca client connects the SafetyMonitor or ObservingConditions device, the live flag is set on. When it disconnects, the flag is set off. |

The problems:

- The group title says "not imaging", but both controls are about *whether alerts go out at all*.
- "Alerts on now" sits under the master **Send alerts** toggle. That reads as if it overrides
  something, or as a question.
- "On while N.I.N.A. is connected" hides its more important half: **alerts stop when the app
  disconnects**. It also names one product, although any Alpaca client triggers it.
- The device only learns about a disconnect when the client says so cleanly. If the imaging app
  crashes, its PC sleeps, or the network drops, the device still thinks the client is connected.
  - Nobody is told, which is the failure the observer most needs to hear about. An unattended
    rig is still running, but nothing is closing the roof for it.
  - The arming logic also never notices the loss.

This spec replaces the schedule controls with one plain-language model. It adds alerts for an
imaging app that disconnects or goes silent, and sets wording rules that can be tested.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Know when the imaging app stops watching the safety monitor (Priority: P1)

An observer leaves N.I.N.A. running an unattended session; N.I.N.A. polls the SQMeter
SafetyMonitor to decide when to park and close. N.I.N.A. then crashes, the PC sleeps, or the
network drops. Within a few minutes the observer gets an urgent notification: "Imaging app stopped
checking the safety monitor - last checked 2 min ago". When it starts checking again, a follow-up
says it's back.

**Why this priority**: This failure is silent and dangerous. Rain will still be detected, but
nothing will close the roof. It is the user's explicit request, and no setting today covers it.

**Independent Test**:
1. Connect any Alpaca client (N.I.N.A., ConformU, or `curl` PUT `connected=true`), then poll
   `issafe`.
2. Stop polling without disconnecting.
3. After the configured time, check that an "Imaging app stopped checking" alert arrives on the
   enabled channels.
4. Poll again and check that an "Imaging app is back" alert arrives.

**Acceptance Scenarios**:

1. **Given** a client has connected the SafetyMonitor and is polling, **When** no request reaches
   that device for longer than the "Silent for" time (default 2 minutes), **Then** an "Imaging app
   stopped checking" alert is sent once. It names the device and when it was last checked.
2. **Given** that alert was sent, **When** any request reaches the device again, **Then** an
   "Imaging app is back" alert is sent, at its own level.
3. **Given** a client disconnects cleanly (`connected=false` or `disconnect`), **When** that
   happens, **Then** an "Imaging app disconnected" alert is sent, if its level isn't Off. It is
   Off by default, because ending a session is normal. No "stopped checking" alert follows.
4. **Given** a client connected only the ObservingConditions device, **When** it goes silent,
   **Then** the alert names the weather device. Clients poll weather less often than safety, so the
   SafetyMonitor and ObservingConditions devices have separate silence times.
5. **Given** both devices are connected and only one goes silent, **When** the time passes, **Then**
   the alert names only that one.
6. **Given** the client lost alert has its level, sound and wording customised, **When** it fires,
   **Then** it behaves like every other event (spec 008): levels, sounds, templates, stacking,
   cooldown, the recent list and channel results.
7. **Given** no client has connected since the device started, **When** time passes, **Then** no
   client alert is ever sent. The device never alerts about a session that never started.

---

### User Story 2 - Understand at a glance when alerts are sent (Priority: P1)

An observer opens Settings → Alerts and can tell, without reading any help text:

- whether alerts are being sent right now, and if not, why;
- what decides that;
- what each control will do before they touch it.

**Why this priority**: The current wording confused the maintainer, a native English speaker. A
safety alert that the observer believes is on, but is actually paused, is as bad as having none.

**Independent Test**: Show the Alerts card to someone who hasn't seen it. Ask:
- "Will you get a rain alert right now?"
- "What happens to alerts when N.I.N.A. disconnects?"
- "How do you stop alerts for tonight?"

They should answer all three correctly, reading only the labels.

**Acceptance Scenarios**:

1. **Given** the Alerts card, **When** it is shown, **Then** it has a **"When to send"** choice with
   two options:
   - **"Any time"**
   - **"Only while an imaging app is connected"**
2. **Given** the Alerts card, **When** it is shown, **Then** a status line states the current state
   in words, with the reason and since when, for example:
   - "Sending alerts."
   - "Paused - the imaging app disconnected at 05:42. Alerts resume when it connects again."
   - "Paused by you at 21:04 (Pause button)."
   - "Paused from Home Assistant at 21:04."
   - "Waiting for an imaging app to connect - nothing is sent until then."
3. **Given** alerts are sending, **When** the status line is shown, **Then** there is a
   **"Pause alerts"** button. While paused there is a **"Resume alerts"** button. There is no toggle
   labelled as a question or as "on now".
4. **Given** "Only while an imaging app is connected", **When** a client connects either device,
   **Then** alerts resume. **When** it disconnects cleanly, **Then** alerts pause. The status line
   says so, with the time.
5. **Given** "Only while an imaging app is connected", **When** the client goes silent instead of
   disconnecting, **Then** alerts **keep being sent**, including the "stopped checking" alert. A
   crash is not the end of a session; it is when weather alerts matter most.
6. **Given** any mode, **When** the observer presses Pause or Resume, **Then** it takes effect
   straight away and lasts until the next Pause or Resume. In "Only while an imaging app is
   connected" mode, it also ends at the next connect or disconnect. The status line names what
   paused it.
7. **Given** Pause and Resume, **When** they come from Home Assistant or a script (MQTT
   `<topic>/alerts/armed/set`, `POST /api/alerts/arm` and `/disarm`), **Then** the UI and status
   line reflect them within one status update.

---

### User Story 3 - Settings and integrations keep working after the update (Priority: P2)

After updating, the observer's existing choices produce the same behaviour. Home Assistant
switches and scripts built on the arm/disarm API keep working.

**Why this priority**: People shouldn't have to reconfigure their alerts.

**Independent Test**:
1. Start from a v0.2.0-beta.3 configuration with `armWithAlpaca` set to true, and alerts disarmed.
2. Update.
3. Check that the mode is "Only while an imaging app is connected", the state is paused, and the
   MQTT switch still toggles it.

**Acceptance Scenarios**:

1. **Given** `armWithAlpaca: true`, **When** the new firmware loads it, **Then** the mode is
   "Only while an imaging app is connected". `false` becomes "Any time".
2. **Given** the persisted armed flag was off, **When** the new firmware starts, **Then** alerts
   are paused, and the stated reason is "Paused (before the update)".
3. **Given** MQTT `<topic>/alerts/armed` and `/set`, and REST `/api/alerts/arm`, `/disarm` and
   `/armed`, **When** they are used, **Then** they behave exactly as before. "Armed" means sending
   and "disarmed" means paused. The REST armed document gains `mode`, `reason` and `since` fields.
4. **Given** the user downgrades to an older firmware after saving settings, **When** the old
   firmware loads them, **Then** it still finds a meaningful `armWithAlpaca`, because the new
   firmware keeps writing it alongside the new setting.

---

### User Story 4 - Try it in the demo (Priority: P3)

A visitor to demo.sqmeter.dev can see both the client lost alerts and the schedule model working.

**Why this priority**: The demo runs the device's own code (spec 016). This feature is easy to
misunderstand without seeing it.

**Independent Test**:
1. In the demo, press "Imaging app connects", then "Imaging app goes silent".
2. With the 10× clock on, see the "stopped checking" alert under the bell within a minute.
3. Press "Imaging app disconnects" and see alerts pause with the reason.

**Acceptance Scenarios**:

1. **Given** the demo, **When** the Demo panel is open, **Then** it offers a simulated imaging
   app: connect, go silent, resume checking, and disconnect. While connected, it polls the emulated
   device's SafetyMonitor every few seconds and its ObservingConditions every minute, as N.I.N.A.
   does.
2. **Given** the demo, **When** the client alerts fire, **Then** they come from the same device
   code as on the hardware. Delivery is simulated, as for every other demo alert.

---

### Edge Cases

- **"Only while an imaging app is connected" with Alpaca off** (amended by spec 020, D-12).
  - No imaging app can connect, so holding alerts would hold them forever.
  - The choice is kept but reported inactive ("Inactive - Alpaca is off"), and alerts go out any
    time until Alpaca is back on. The three imaging-app events are inactive too (020 D-37).
- **The device restarts mid-session.**
  - The device forgets connections; the client usually keeps polling and may not reconnect.
  - A request after a restart, from a client that never connected since, counts as the client
    watching. Some clients poll without re-sending `connected=true`.
  - Silence after a restart is only alerted if a request has arrived since the restart.
  - "Is back" is not sent after a restart.
- **Several clients at once** (N.I.N.A. plus a weather dashboard):
  - Silence is judged per device, from the newest request from any client.
  - One client still polling means that device is being watched.
  - A clean disconnect pauses alerts, in that mode, only when no device is left connected.
- **A client connects but never polls** (connects, then sits idle): the silence time counts from
  the connect.
- **Alpaca is switched off in Settings while a client is connected**: client tracking stops, and
  no "stopped checking" alert is sent. The state the client sees is spec 007's concern.
- **The imaging app goes silent while alerts are paused by hand**: nothing is sent. The pause is
  respected. The status line still shows the client as silent.
- **The cooldown** applies per event type, as for other events. A client that flaps between silent
  and back doesn't produce more than one alert per cooldown period. A change held back during the
  cooldown is sent when the cooldown ends (spec 008 behaviour).
- **The "Only when it's dark" settings** don't hold back client alerts. Sessions can start at
  dusk, and a silent client matters at any hour.
- **The device has no clock** (no NTP or GPS): client alerts still work, because silence is
  measured on the device's uptime clock. The "last checked" time is given as "N min ago" instead
  of a clock time.
- **"Silent for" is set shorter than the client's poll interval** (for example 30 s for
  ObservingConditions with N.I.N.A. polling weather every 60 s): the setting's hint warns about
  this. The minimum allowed value is 30 s.

## Requirements *(mandatory)*

### Functional Requirements

#### Schedule model

- **FR-001**: Alerts MUST have a sending mode with exactly two values:
  - **Any time**: alerts are sent unless paused.
  - **Only while an imaging app is connected**: alerts are sent while any Alpaca client has the
    SafetyMonitor or ObservingConditions device connected. They pause when the last one disconnects
    cleanly.
- **FR-002**: Independently of the mode, alerts MUST be pausable and resumable. Pause and resume
  come from the UI button, REST (`/api/alerts/disarm` and `/arm`) or MQTT
  (`<topic>/alerts/armed/set`).
  - A pause or resume lasts until the next pause or resume.
  - In "Only while an imaging app is connected" mode, it also ends at the next client connect or
    disconnect.
- **FR-003**: The device MUST keep, and report, why alerts are paused or sending and since when.
  - Reasons: paused from the web UI, from REST, or from MQTT; the imaging app disconnected; waiting
    for an imaging app to connect; paused before an update.
  - Reported in `/api/alerts/armed` (new `mode`, `reason` and `since` fields alongside `armed`) and
    in `/api/status`.
- **FR-004**: A client that goes silent MUST NOT pause alerts. Only a clean disconnect (Alpaca
  `connected=false` or `disconnect`) does.
- **FR-005**: The master **Send alerts** toggle (`alerts.enabled`) keeps its meaning: the alert
  feature is on or off. Off overrides everything. When it is off, the mode and pause controls
  aren't shown.
- **FR-006**: The "Only when it's dark" limits for sky and safety alerts (spec 008, FR-004) are
  unchanged and still apply on top of the mode. Client alerts are exempt from them.

#### Client lost alerts

- **FR-007**: The device MUST track, for each Alpaca device (SafetyMonitor and
  ObservingConditions), whether a client has it connected and when the last request for that
  device arrived. Any method counts, from any client. Time is measured on the device's uptime clock.
- **FR-008**: The device MUST raise **"Imaging app stopped checking"** (`client_lost`) when a device
  has a client watching but no request has arrived for longer than that device's **"Silent for"**
  time. "Watching" means connected, or polled since the last restart.
  - Defaults: SafetyMonitor 2 minutes, ObservingConditions 10 minutes.
  - Allowed range: 30 seconds to 60 minutes.
  - It fires once per loss.
- **FR-009**: The device MUST raise **"Imaging app is back"** (`client_back`) on the first request
  for a device after a `client_lost` for that device.
- **FR-010**: The device MUST raise **"Imaging app disconnected"** (`client_disconnected`) when a
  client cleanly disconnects a device. This happens before any resulting pause takes effect, so it
  can be delivered.
- **FR-011**: These three events MUST be configurable like every other event (spec 008): level,
  Pushover sound, title and message templates. They join the same stacking, cooldown, recent list,
  channels, test buttons and Bluetooth "wake me" behaviour.
  - Default levels: `client_lost` Urgent, `client_back` Quiet, `client_disconnected` Off.
- **FR-012**: The templates MUST offer these variables (snake_case like every other template
  variable; see research.md D11):
  - `{device}`: "safety monitor" or "weather device"
  - `{silent_for}`: for example "2 min"
  - `{last_checked}`: a local clock time, or "N min ago" without a clock
  - `{client_id}`: the Alpaca ClientID last seen, if any
  - The default wording MUST NOT name a specific product.
- **FR-013**: No client event MUST fire unless a client was watching the device since the last
  restart.
- **FR-014**: Each device's client state (connected, watching, last checked, silent) MUST be
  shown in `/api/status` and on the Alpaca page. Example: "Safety monitor: connected, last checked
  3 s ago".
- **FR-015**: The client event rules MUST live in the device's shared, host-testable logic (the
  same code the demo runs). They need native tests for:
  - silence, recovery and clean disconnect;
  - the restart case;
  - several clients;
  - cooldown interaction;
  - the mode and pause rules in FR-001 to FR-004.

#### Wording

- **FR-016**: The UI MUST use the labels and help text in the **Wording** table below. Any change
  goes through this spec.
- **FR-017**: Every label MUST pass these rules, enforced by a UI test over the rendered Alerts card:
  - **No question labels**: no label ends with "?".
  - **No negations in labels**: labels don't contain "not", "no", "don't" or "off when". Negatives
    go in help text.
  - **States its effect**: a toggle or option label states what happens when it is selected, as
    an affirmative phrase.
  - **No product names in labels**: product names appear only in hints, as examples ("e.g.
    N.I.N.A.").
  - **Status in words**: the current state is a sentence with a reason, not only a toggle position.
- **FR-018**: The alerts documentation (`docs/`) MUST use the same labels and explain the model:
  - the two modes;
  - pause and resume;
  - silence versus disconnect;
  - the three client events.

#### Migration and compatibility

- **FR-019**: Existing configurations MUST migrate: `armWithAlpaca: true` becomes "Only while an
  imaging app is connected", and `false` becomes "Any time". The persisted armed flag carries over
  as the pause state.
- **FR-020**: The device MUST keep writing `armWithAlpaca`, consistent with the mode, so an older
  firmware reads the nearest equivalent after a downgrade.
- **FR-021**: REST and MQTT arm/disarm interfaces MUST stay backward compatible. New fields are
  additive, and the data-interface contracts (spec 013) and schemas are updated.
- **FR-022**: The new settings MUST fit in the device's persistent settings storage with every
  event template filled to its maximum length. They MUST NOT push an existing configuration past
  the storage limit.

#### Demo

- **FR-023**: The demo MUST offer a simulated imaging app: connect, go silent, resume and
  disconnect, polling at N.I.N.A.'s typical rates. Requests the visitor makes to the emulated
  Alpaca API also count as client activity.

### Wording

| Where | Label | Help text |
|---|---|---|
| Card | **Send alerts** (master toggle, unchanged) | Push notifications sent by the device itself. |
| Group title | **When to send** | — |
| Option 1 | **Any time** | Alerts go out whenever something happens, unless you pause them. |
| Option 2 | **Only while an imaging app is connected** | Alerts start when an imaging app (e.g. N.I.N.A.) connects the safety monitor or weather device, and stop when it disconnects. If it stops responding without disconnecting, alerts keep coming - and you're told it went quiet. |
| Status (sending) | "Sending alerts." | — |
| Status (paused) | "Paused - {reason} at {time}." + "Alerts resume when {condition}." where there is one | — |
| Status (waiting) | "Waiting for an imaging app to connect - nothing is sent until then." | — |
| Button | **Pause alerts** / **Resume alerts** | Takes effect straight away. Home Assistant and scripts can do the same: MQTT `<topic>/alerts/armed/set`, or `POST /api/alerts/disarm` and `/arm`. |
| Event row | **The imaging app stops checking** | No request reached the safety monitor or weather device for the time below. A crash, a sleeping PC or a network drop. |
| Event row | **The imaging app is back** | It started checking again after going quiet. |
| Event row | **The imaging app disconnects** | It disconnected normally, for example at the end of a session. |
| Field | **Silent for - safety monitor** (default 2 min) | How long without a request before you're told. Imaging apps usually check the safety monitor every few seconds. |
| Field | **Silent for - weather device** (default 10 min) | Imaging apps check weather less often; keep this longer than their weather interval. |
| Default title / message | "Imaging app stopped checking" / "No request to the {device} for {silent_for} - last checked {last_checked}." | — |
| Default title / message | "Imaging app is back" / "The {device} is being checked again." | — |
| Default title / message | "Imaging app disconnected" / "The {device} was disconnected." | — |

### Key Entities

- **Sending mode**: `any` or `whileConnected`. It is persisted, and replaces `armWithAlpaca`,
  which is still written for compatibility.
- **Pause state**:
  - sending or paused;
  - the reason: `user-ui`, `user-rest`, `user-mqtt`, `client-disconnected`,
    `waiting-for-client` or `migrated`;
  - since: uptime, and clock time when known.
  - It is persisted across restarts, as the armed flag is today.
- **Client watch (per Alpaca device)**:
  - connected;
  - watching (connected, or polled since the restart);
  - last request time (uptime);
  - last ClientID;
  - silent (lost alert raised and not yet recovered).
- **Event settings**: `clientLost`, `clientBack` and `clientDisconnected`, with the same fields as
  other events. Plus `clientSilentSafetySeconds` and `clientSilentWeatherSeconds`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: When an imaging app goes silent, the observer is notified within the "Silent for"
  time plus 10 seconds. This is verified on hardware with N.I.N.A. or ConformU by killing the
  client process, and in native tests.
- **SC-002**: When a client disconnects cleanly, no "stopped checking" alert is sent, in 100% of
  test runs.
- **SC-003**: Three people who haven't seen the Alerts card answer the three questions in
  User Story 2 correctly from the labels alone.
- **SC-004**: The automated wording test passes over every label on the Alerts card: no question
  marks, no negations, no product names.
- **SC-005**: After an update from v0.2.0-beta.3, the sending behaviour and the HA/MQTT switch are
  unchanged for both `armWithAlpaca` values and both armed states (4 of 4 combinations).
- **SC-006**: In the demo, a visitor can trigger, observe and recover from a silent imaging app in
  under 1 minute with the 10× clock.

## Assumptions

- The SafetyMonitor and ObservingConditions devices are the only Alpaca devices (spec 007). A
  client is anything that sends Alpaca device requests; management requests (`/management/...`)
  don't count as checking.
- Alpaca requests carry no client name, so the device reports devices, not applications. `ClientID`
  is shown when present, but it's an arbitrary number chosen by the client.
- N.I.N.A.'s defaults:
  - It checks the safety monitor every few seconds and weather every 60 seconds or more.
  - Hence the 2-minute and 10-minute defaults. The minimum of 30 s avoids false alarms from
    ordinary network hiccups.
- "Paused before the update" is a one-time reason, shown until the next pause or resume.
- Client events are judged per device, not per client. Telling apart several clients on the same
  device isn't needed for a single-observer setup and isn't reliable without client names.
- The existing REST and MQTT naming (arm, disarm, armed) stays as it is for compatibility. Only
  the UI and documentation change to "pause" and "resume".
- Bluetooth "wake me" behaviour for the new events comes from spec 008 and 009 unchanged.

## Dependencies

- Spec 007 (ASCOM Alpaca): connection handling. Recording request times per device is added to
  the shared Alpaca logic.
- Spec 008 (Alerts): events, levels, templates, stacking, cooldown, arming. This spec amends its
  FR-005 and the "When you're not imaging" UI.
- Spec 013 (Data interfaces): `/api/alerts/armed` and `/api/status` contracts.
- Spec 016 (Demo device emulation): the demo runs the same core. The Demo panel gains the simulated
  imaging app.

## Out of Scope

- Telling multiple imaging applications apart, or naming them.
- Alerting on a silent MQTT or Home Assistant subscriber.
- Taking action on the client's behalf, for example closing a roof when the client goes silent.
  The SQMeter is a monitor, not a controller.
