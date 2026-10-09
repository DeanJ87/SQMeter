# Feature Specification: Dashboard at a glance

**Feature Branch**: `spec/025-dashboard-at-a-glance`

**Created**: 2026-10-09

**Status**: Draft

**Input**: User description: "the homepage is definitely missing some information given the new things added, ipv6, imaging status etc like if you have rules for you need nina connected, you can't see that at a glance if it is or not, if it's stale etc. This needs a spec first, to ensure we don't regress."

## Context

The dashboard (spec 010) was designed before specs 015 and 020–021 added device state that decides
whether the observatory is safe to use and whether anyone will be told when it isn't. Today much of
that state reaches the device's API but not the dashboard, or only reaches it behind a click.

An audit of `web/src/components/Dashboard.tsx`, `SafetyCard.tsx`, `AlertsBell.tsx` and the API
documents (`/api/status`, `/api/sensors`, `/api/safety`, `/api/alerts/armed`,
`/api/settings/effective`) against specs 001–024 found these gaps:

| State | In the API | On the dashboard today |
|---|---|---|
| Imaging app (Alpaca client) connected, watching, silent, last checked (021) | `status.alpaca.clients.*` | Not shown anywhere on the dashboard |
| Alerts sending, paused or waiting, with reason and since (021) | `status.alerts`, `/api/alerts/armed` | Bell icon crossed out; the reason and time are only in the flyout |
| Send mode "Only while an imaging app is connected" and whether it is in effect (020 D-12, 021) | `status.alerts.mode`, settings-effective D-12 | Not shown |
| Safety rules not in effect because a sensor is off (020) | `safety.rulesNotInEffect` | Not shown |
| Other settings switched on but not in effect, e.g. an alert channel that can't work (020) | `/api/settings/effective` | Not shown (only on each Settings row) |
| A sensor that is switched on but has failed, is missing or is stale (001–004, 010) | `readings.<group>.status`, `status.sensors.*` | The card disappears (spec 010 FR-001), so a failure looks the same as "not fitted" - except rain, which shows a Stale pill |
| Data freshness overall: live, stale, stream quiet, disconnected (010, #103) | `readings.dataStale`, WebSocket timing | Only a pill on the Sky Quality card; nothing if that card is hidden |
| Rain held by the clear delay, and time until it clears (003) | `readings.rain.raining` (held), no remaining time | "Raining: Yes" with no indication it's the hold, nor how long is left |
| IPv6 addresses (015) | `status.wifi.ipv6` | Not shown |
| mDNS name (013, 014) | `status.wifi.hostname`, `mdns` | Not shown |
| MQTT connected / disconnected (013) | `status.mqtt` | Not shown |
| Clock valid and time source (005) | `readings.timeValid`, `status.ntp`, `status.time` | Not shown |
| Location source and whether darkness is known (005) | `status.sky.locationSource`, `nightKnown` | Not shown (Sun & Moon hides without a location) |
| Bluetooth phone alarm active, paired phones (009) | `status.ble` | Not shown |
| Firmware update available (012) | Updates page only | Version shown, availability not |
| Demo: the device clock or conditions differ from reality (019) | Demo panel | Only in the Demo panel |

Two of these hide dangerous situations:

- **Waiting for an imaging app.** With "Only while an imaging app is connected", alerts stop as soon
  as the app disconnects. The dashboard gives no sign of it, so an observer can believe they are
  protected when nothing will be sent.
- **A failed sensor vanishes.** A dead cloud sensor removes the Cloud card, and nothing on the page
  says a sensor the safety rules depend on is gone. The verdict does turn unsafe, but the
  dashboard looks as if the sensor was never fitted.

This spec defines what the dashboard must show, when, and how that is kept from regressing as
features are added.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See at a glance whether anyone will be warned (Priority: P1)

An observer opens the dashboard before leaving the rig. Without clicking anything they can tell
whether alerts will go out right now and, if not, why: paused by them, paused by a script, waiting
for an imaging app to connect, or no channel able to send. If an imaging app is meant to be
watching, they can see whether it is connected and when it last checked.

**Why this priority**: The send mode from spec 021 can silently stop alerts. Not knowing that is
the failure this whole feature exists to prevent.

**Independent Test**: In the demo, set send mode to "Only while an imaging app is connected" with
no simulated app connected; the dashboard reads "Alerts waiting for an imaging app - nothing is
sent until one connects" without opening any menu. Connect the simulated app; it reads "Sending
alerts" and shows the app as watching. Silence the app; it reads "Imaging app stopped checking -
last checked 2 min ago".

**Acceptance Scenarios**:

1. **Given** alerts are enabled and sending, **When** the dashboard loads, **Then** a calm
   one-line "Sending alerts" state is visible and nothing alarming is shown.
2. **Given** alerts are paused (by the user, a script or MQTT), **When** the dashboard loads,
   **Then** it shows "Paused" with who paused them and since when, and a Resume action.
3. **Given** send mode is "Only while an imaging app is connected" and no app is connected,
   **When** the dashboard loads, **Then** it says alerts are waiting for an imaging app.
4. **Given** that send mode and Alpaca is switched off, **When** the dashboard loads, **Then** it
   says the mode is not in effect because Alpaca is off and alerts go out any time (spec 020
   D-12), with a link to fix it.
5. **Given** an imaging app was connected and goes silent past the configured time, **When** the
   dashboard updates, **Then** the app is shown as "stopped checking" with the time of its last
   check, prominently, even while alerts are paused.
6. **Given** alerts are enabled but every enabled channel is inactive (e.g. MQTT channel with MQTT
   off and no other channel), **When** the dashboard loads, **Then** it says no channel can send,
   with the reason.

---

### User Story 2 - Never mistake a failed sensor or stale data for a quiet night (Priority: P1)

A sensor that is switched on stops answering, or the data stream goes quiet. The dashboard says so
on the page - the sensor's card stays, marked with its fault and how old its last reading is - and
an overall freshness state is always visible, whichever cards are shown.

**Why this priority**: Safety decisions read these sensors; a vanished card reads as "nothing to
see". Principle I (fail-safe) applies to the display as much as to the verdict.

**Independent Test**: In the demo, mark the IR sensor "not responding" (spec 019); the Cloud and IR
cards remain with a "Not responding" state and the age of the last reading, and the safety card
lists the reason. Stop the stream (close the WebSocket); the page shows "Updates stopped" within the
quiet threshold.

**Acceptance Scenarios**:

1. **Given** a sensor is enabled and its status is `error`, `missing` or `stale`, **When** the
   dashboard renders, **Then** its card is shown in a fault state with the status in words and the
   age of its last good reading (when known), not removed.
2. **Given** a sensor is switched off in Settings, **When** the dashboard renders, **Then** its card
   is not shown (unchanged from spec 010).
3. **Given** the device reports `dataStale` or the stream has been quiet past the threshold, **When**
   the dashboard renders, **Then** an overall freshness state ("Stale" / "Updates stopped" /
   "Disconnected") is visible at the top of the page, independent of which cards are shown.
4. **Given** the device is unreachable, **When** the dashboard renders, **Then** it says so and
   shows the last values greyed with their age, never as current.

---

### User Story 3 - Know why the verdict is what it is, and when it will change (Priority: P1)

The safety card already lists unsafe reasons and a "Safe in Ns" countdown. It must also show safety
rules that are configured but not in effect (spec 020 `rulesNotInEffect`), and, while rain holds the
verdict, how long until the rain clear delay ends.

**Why this priority**: An observer deciding whether to open the roof needs the whole picture: why it
is unsafe, which protections are silently off, and when it is expected to change.

**Independent Test**: In the demo, switch the rain sensor off while the rain rule is on; the safety
card says "Rain rule not in effect - rain sensor is off". Start rain then stop it; the card shows
"Rain held - clears in 14 min" counting down.

**Acceptance Scenarios**:

1. **Given** `rulesNotInEffect` is non-empty, **When** the dashboard renders, **Then** each listed
   rule is shown on the safety card with its reason and a link to the setting.
2. **Given** rain has stopped but the clear delay is holding the verdict, **When** the dashboard
   renders, **Then** it shows the hold and the time remaining (requires the device to report the
   remaining time - see FR-012).
3. **Given** the verdict is raw-safe but within the safe delay, **When** the dashboard renders,
   **Then** the countdown is shown (as today).

---

### User Story 4 - Device, network and time health without visiting System (Priority: P2)

At a glance: Wi-Fi network and signal, IPv4 and IPv6 addresses, the `.local` name, MQTT
connection, whether the clock is set and from which source, where the location comes from,
Bluetooth phone alarm state, and firmware version with "update available" when a check has found
one.

**Why this priority**: These explain many other failures (no clock → no darkness; MQTT down → no
telemetry) but are not themselves safety-critical, so problems are prominent and healthy values
stay compact.

**Independent Test**: In the demo, enable IPv6 and MQTT; the Device & Network card shows the IPv6
address(es), the `.local` name and "MQTT connected". Make the clock invalid (spec 019 time
controls); a "Clock not set - darkness unknown" note appears at the top.

**Acceptance Scenarios**:

1. **Given** IPv6 is enabled and the device has addresses, **When** the dashboard renders, **Then**
   the Device & Network card lists them (global before link-local), wrapping at 320 px.
2. **Given** MQTT is enabled, **When** the dashboard renders, **Then** its state reads
   "Connected" / "Disconnected - retrying" / "Can't connect: reason"; when MQTT is off nothing
   about it is shown.
3. **Given** the clock is not set, or no location is known, **When** the dashboard renders,
   **Then** a note explains the consequence (darkness and night-only rules can't work) and links
   to Time & Location.
4. **Given** a Bluetooth build with phones paired, **When** the phone alarm is active, **Then** the
   dashboard shows it with an acknowledge action; when no phone is paired but "Wake me" is chosen
   for any event, it says so.
5. **Given** the last update check found a newer release, **When** the dashboard renders, **Then**
   the firmware row shows "Update available: vX" linking to Updates. The dashboard itself never
   contacts GitHub.

---

### User Story 5 - A regression is caught before it ships (Priority: P1)

A developer adds a field to `/api/status` or a dependency to the settings catalogue. CI fails until
they record in the dashboard inventory whether and when it is shown - or deliberately not shown,
with a reason. Every inventory item has an automated test that puts the demo device into the
qualifying state and checks the dashboard shows it.

**Why this priority**: The user's explicit ask: "to ensure we don't regress". The gaps above
happened because nothing tied new device state to the dashboard.

**Independent Test**: Add a dummy field to the status schema without an inventory entry; the check
fails naming the field. Remove the "imaging app" item from the dashboard; its Playwright test fails.

**Acceptance Scenarios**:

1. **Given** an unmapped field in the status, readings, safety or alerts-armed schema, or an
   unmapped settings-dependency id, **When** CI runs, **Then** the inventory check fails with the
   field and how to fix it.
2. **Given** an inventory item marked shown, **When** its test drives the demo into the qualifying
   state, **Then** the item is visible within the stated time, and hidden when its rule says so.

---

### User Story 6 - The demo shows the same thing (Priority: P3)

The demo dashboard is the device dashboard (spec 016). When demo conditions move the device clock
or put it in an unusual state, a small "Demo" marker on the dashboard says so, linking to the Demo
panel.

**Acceptance Scenarios**:

1. **Given** the demo device clock is not the real time (spec 019), **When** the dashboard renders,
   **Then** the time shown is labelled as the demo device's time.

### Edge Cases

- **Everything healthy**: the at-a-glance area collapses to a single calm line (e.g. "Safe ·
  Sending alerts · Live"), per Principle V (quiet UI). Healthy details stay in their cards.
- **Several problems at once**: shown in a fixed priority order: device unreachable → safety
  verdict and its reasons → alerts not going out → imaging app stopped checking → sensor faults and
  stale data → settings not in effect → clock/location unknown → network and integrations →
  update available. No more than the top few take full prominence on a phone; the rest are
  reachable in one tap, with a count.
- **Older firmware** lacking a field (e.g. no `alpaca.clients`): the item is hidden, never shown as
  a false "OK" or a crash. Unknown is shown as unknown where the item is safety-relevant.
- **Alerts disabled entirely** (master switch off): the dashboard says "Alerts are off" once; the
  imaging-app item is still shown if an app is connected (it matters to the safety monitor).
- **Imaging app connected but alerts mode is "Any time"**: the app state is shown compactly (it
  still matters whether N.I.N.A. is watching the safety monitor), prominently only if it goes silent.
- **Rain sensor off but rain rule on**: covered by `rulesNotInEffect`; the rain card stays hidden.
- **IPv6 enabled but no address yet**: "IPv6: no address yet", not an empty row.
- **Very narrow screens (320 px) and 400 % zoom**: nothing at a glance hides a control (spec 022,
  019 SC-007); long addresses wrap.
- **Translations (023)**: German/French run long; the at-a-glance line wraps instead of truncating
  meaning. Arabic is right-to-left; times, addresses and numbers stay left-to-right.

## Requirements *(mandatory)*

### Functional Requirements

**Inventory and enforcement**

- **FR-001**: A **dashboard information inventory** MUST exist as a machine-readable file in the
  repo. Each entry has an id, the state it shows, the API field(s) it reads, the spec requirement
  that needs it, its **visibility rule** (Always / When relevant: condition / On demand), its
  priority, and where it appears.
- **FR-002**: Every field in the status, readings, safety and alerts-armed contract schemas, and
  every id in the settings-dependency catalogue, MUST be mapped either to an inventory entry or to
  a "not shown on the dashboard" entry with a reason (e.g. "diagnostics - System page"). A CI check
  MUST fail on any unmapped field or id, naming it and the fix.
- **FR-003**: Every inventory entry marked shown MUST have an automated test that drives the demo
  device (spec 019 conditions, spec 021 simulated imaging app) into the qualifying state and asserts
  the item is visible with the expected text, and a test asserting it is hidden when its rule says
  so. The check of FR-002 MUST fail if a shown entry has no test.
- **FR-004**: The coding standard MUST gain a rule (DASH-01) that a change adding device state
  visible to users updates the inventory, and the constitution's quality gate MUST reference it, so
  `/speckit-converge` checks it for every future spec.

**At-a-glance area**

- **FR-005**: The dashboard MUST have an at-a-glance area at the top, above the cards, that is
  always present and shows, in the priority order of the Edge Cases: connection/freshness, safety
  verdict, alert sending state, and any active problem items. When everything is healthy it MUST
  be a single compact line.
- **FR-006**: Anything that can block observing (unsafe verdict, a sensor the safety rules depend
  on failing, data stale) or silence alerts (paused, waiting for an imaging app, no channel able to
  send, imaging app stopped checking) MUST be visible in the at-a-glance area without any click,
  tap or hover.
- **FR-007**: Each problem item MUST say what is wrong, since when (where known), and the
  consequence in plain words, with a direct link or action to fix it (e.g. Resume, open the
  setting).

**Alerts and imaging app (021, 020)**

- **FR-008**: The alert sending state MUST be shown as a sentence using spec 021's wording
  (Sending alerts / Paused - by you / by a script / before the update / Waiting for an imaging app /
  The imaging app disconnected at HH:MM), with since-time, and a Resume/Pause action.
- **FR-009**: The imaging app state (per Alpaca device: SafetyMonitor and ObservingConditions)
  MUST be shown when send mode is "Only while an imaging app is connected", or a client is
  connected, or a client has been seen since boot. It reads Watching (last checked N s ago) /
  Stopped checking (last checked HH:MM) / Disconnected at HH:MM / Not connected. "Stopped checking"
  is a problem item (FR-006).
- **FR-010**: When the send mode is not in effect (Alpaca off, spec 020 D-12), the dashboard MUST
  say so and that alerts go out any time.
- **FR-011**: Settings reported inactive by `/api/settings/effective` that affect alerts or safety
  MUST be summarised ("2 settings not in effect") with the list one tap away; ones that silence all
  alert channels are problem items.

**Safety and sensors (006, 003, 001–004)**

- **FR-012**: The safety card MUST show `rulesNotInEffect` with reasons, and the remaining rain
  clear-delay time while rain holds the verdict. The device MUST report that remaining time (new
  readings/safety field, documented in the contracts) so the dashboard need not guess.
- **FR-013**: An enabled sensor whose status is not `ok` MUST keep its card, in a fault state with
  the status in words and the age of its last good reading. This supersedes spec 010 FR-001
  ("cards only for enabled, working hardware") for enabled sensors; switched-off sensors stay
  hidden.
- **FR-014**: Overall freshness MUST be shown in the at-a-glance area: Live / Stale (device
  `dataStale`) / Updates stopped (stream quiet past the threshold) / Disconnected. It MUST NOT
  depend on any one card being visible.

**Device, network, time (005, 009, 012, 013, 015)**

- **FR-015**: The Device & Network card MUST show Wi-Fi network and signal, IPv4 address, IPv6
  addresses when enabled (or "no address yet"), the `.local` name when mDNS is on, and MQTT state
  when MQTT is enabled.
- **FR-016**: An unset clock or unknown location MUST be a problem item naming the consequence
  (darkness unknown, night-only rules and alerts can't apply), linking to Time & Location. The time
  source (NTP/GPS) and location source (Settings/GPS) MUST be visible on demand.
- **FR-017**: On Bluetooth builds, an active phone alarm MUST be a problem item with an acknowledge
  action; "Wake me" chosen with no paired phone MUST be shown (via FR-011).
- **FR-018**: The firmware row MUST show "Update available" when the device's last update check
  found a newer release on the configured track. The dashboard MUST NOT trigger network checks
  itself.

**Quality**

- **FR-019**: All dashboard text MUST come from translation keys (spec 023); states MUST be
  conveyed in words, not colour alone, and announced to screen readers only on important changes
  (verdict, alerts stopped, imaging app stopped checking, disconnected) - not on every update
  (spec 022).
- **FR-020**: At 320 px the at-a-glance area MUST NOT hide any control, and MUST NOT push the safety
  verdict below the first screen.
- **FR-021**: The same state MUST read the same on the dashboard, Settings, the Alpaca page, the
  alerts flyout and alert messages (shared wording source).
- **FR-022**: The demo MUST label a shifted device clock and show a "Demo" marker linking to the
  Demo panel.
- **FR-023**: The dashboard guide in the docs MUST describe every inventory item, with screenshots
  regenerated by the screenshot test, and diagrams updated where affected (spec 024).

### Key Entities

- **Inventory entry**: id, label key, source fields, required by (spec FR), visibility rule,
  priority, location (at-a-glance / card / on demand), test id.
- **Not-shown entry**: source field or dependency id, reason, where it is shown instead (if
  anywhere).
- **Problem item**: an inventory entry currently in a state that blocks observing, silences alerts
  or degrades data; has a severity, since-time, consequence text and fix action.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: From the dashboard alone, without clicking, a user can answer within 5 seconds: is it
  safe and why; will alerts go out and if not why; is the imaging app watching; is the data fresh;
  is anything switched on but not working. Checked in a task-based test on the demo for each
  state.
- **SC-002**: 100 % of status/readings/safety/alerts-armed schema fields and settings-dependency ids
  are mapped in the inventory; CI fails on the first unmapped one.
- **SC-003**: 100 % of shown inventory entries have passing show and hide tests in CI.
- **SC-004**: When everything is healthy, the at-a-glance area is one line at 1280 px and at most
  two lines at 320 px.
- **SC-005**: A sensor failure, alerts going quiet or an imaging app stopping is visible on the
  dashboard within 5 seconds of the device reporting it.
- **SC-006**: axe reports no violations on the dashboard in any inventory state (spec 022 checks),
  and no layout-overlap test fails at 320/390 px.
- **SC-007**: The device UI bundle grows by no more than 4 KB gzipped for this feature.

## Assumptions

- "At a glance" means visible on load without scrolling on a 1280 × 800 desktop, and the verdict
  plus problem items visible without scrolling on a 390 × 844 phone.
- The rain clear-delay remaining time is not in the API today; adding it (FR-012) is in scope. No
  other new device fields are needed: the rest is already in `/api/status`, `/api/safety`,
  `/api/alerts/armed` and `/api/settings/effective`.
- "Update available" uses the result of the device's own last check (Updates page / spec 012); if
  the device keeps no such result, the dashboard shows it only after the user checks on the Updates
  page in the same browser session.
- Healthy state is compact by design (Principle V); detail stays in cards and on the System page.
- Diagnostics, partitions, heap/stack internals, MQTT topics and flash sizes are deliberately not
  on the dashboard (System page), recorded as not-shown entries.
- The at-a-glance area replaces neither the safety card nor the alerts bell; both remain.
- Card drag-and-drop ordering (spec 010) still applies to cards; the at-a-glance area is fixed at
  the top.

## Dependencies

- Spec 010 (dashboard): FR-001 superseded in part by FR-013 of this spec.
- Spec 016/019 (demo emulation and conditions): drives every inventory test.
- Spec 020 (settings dependencies) and 021 (alert schedule, imaging app tracking): source of the
  most important new state.
- Spec 022 (accessibility), 023 (translations), 024 (diagrams), 017 (coding standards).

## Out of Scope

- Redesigning the cards' contents beyond the fault state, Device & Network additions and safety
  card additions above.
- New alerts or notifications (the dashboard reflects state; alerts are spec 008/021).
- Historical charts of these states.
