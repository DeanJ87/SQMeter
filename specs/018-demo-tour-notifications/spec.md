# Feature Specification: Demo Tour and Real Notifications

**Feature Branch**: `feat/demo-tour` (when implementation starts)

**Created**: 2026-10-08

**Status**: Implemented (PR #98) - converged 2026-10-09

**Input**: User description: "Since the demo runs in the browser, let visitors opt in to real notifications from the demo - fill in their own Pushover / ntfy / MQTT details and receive the demo's alerts - and add walkthrough prompts like a product demo site, guiding them through the device (turn GPS off, set a location, trigger rain, see the alert)."

## Clarifications

### Session 2026-10-08

- Q: Can a browser page send to the notification services directly? → A: Checked 2026-10-08: Pushover (`api.pushover.net`) and ntfy (`ntfy.sh`) both allow requests from any web page. MQTT only works with brokers that offer MQTT over secure WebSockets; webhooks only when the target allows browser requests.

### Session 2026-10-09 (plan)

- Q: How can the security policy allow services "only while real sending is on" (FR-011) on GitHub Pages? → A: It can't
  directly: the policy is a fixed `<meta>` tag and can only be tightened at runtime. It names exactly ntfy.sh and
  api.pushover.net (plus secure WebSockets, already allowed), and the demo's code sends nothing unless the visitor turned
  real sending on in that tab (research R1).
- Q: Webhooks and self-hosted ntfy servers (FR-006)? → A: Shown as unavailable in the demo before the visitor tries:
  allowing any HTTPS host would widen the public page's policy far more than the feature is worth (research R1).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A guided tour (Priority: P1)

A first-time visitor is offered a short tour that points at the real controls and explains what
SQMeter does: the dashboard, the safety verdict, triggering rain and watching the verdict, the bell
and Alpaca react, changing a setting and seeing its effect, and where to go next (docs, build, buy parts).

**Why this priority**: Most visitors don't know what to try; the scenarios and settings only impress
once someone shows them.

**Independent Test**: A new visitor completes the tour in under 3 minutes and can then explain what
the safety verdict and alerts are for.

**Acceptance Scenarios**:

1. **Given** a first visit, **When** the demo loads, **Then** it offers the tour once (dismissable; not shown
   again in that browser after dismissing or finishing).
2. **Given** the tour is running, **When** a step asks the visitor to do something (e.g. press Rain), **Then**
   the step waits for it and moves on when the device reacts, highlighting where to look.
3. **Given** any step, **When** the visitor presses Skip or Esc, **Then** the tour ends and the demo is usable.
4. **Given** the tour, **When** viewed on a phone, **Then** every step fits and points at visible controls.
5. **Given** the Demo panel, **When** the visitor wants the tour again, **Then** "Take the tour" restarts it.

---

### User Story 2 - Get the demo's alerts on my phone (Priority: P2)

A visitor opts in, enters their own Pushover keys or an ntfy topic, triggers rain, and their phone
gets the alert - the same wording a real SQMeter would send.

**Why this priority**: Feeling an alert arrive sells the product better than reading about it.

**Independent Test**: Opt in with an ntfy topic, trigger Rain, receive "Rain detected" on a phone
subscribed to that topic.

**Acceptance Scenarios**:

1. **Given** the alert settings, **When** the visitor turns on "Send real notifications from this demo" and
   confirms what it does, **Then** alerts and tests for the enabled channels are delivered for real.
2. **Given** real sending is off (the default), **When** an alert fires, **Then** nothing is sent and the
   result says "Demo: nothing was sent", as today.
3. **Given** a delivery, **When** it succeeds or fails, **Then** the alert's channel result shows what happened
   (e.g. "Delivered", "Pushover: invalid user key", "This broker doesn't accept browser connections").
4. **Given** a channel that can't work from a browser (a webhook without browser access, an MQTT broker
   without secure WebSockets), **When** the visitor enables it, **Then** the demo says so before they try.

---

### User Story 3 - Safe to offer (Priority: P1)

Real sending can't be used to spam anyone, leak a visitor's keys, or make the site send things on
its own.

**Why this priority**: The demo is public; it must stay harmless (spec 016's guarantee, narrowed
deliberately here).

**Independent Test**: With real sending off, a network check sees no outbound requests (spec 016's
test still passes); with it on, requests go only to the services the visitor configured, from their
own browser.

**Acceptance Scenarios**:

1. **Given** real sending is off, **When** the demo runs, **Then** spec 016's no-outbound guarantee holds unchanged.
2. **Given** real sending is on, **When** alerts fire, **Then** requests go only to the configured services, from
   the visitor's browser, never through any server of ours.
3. **Given** keys the visitor entered, **When** the tab closes, **Then** they're gone (never written to
   storage that outlives the tab, never in URLs or logs).
4. **Given** a scenario left running, **When** alerts would fire repeatedly, **Then** real deliveries are capped
   (rate limit) so a visitor can't flood their own or anyone's channel.

---

### Edge Cases

- Pushover emergency (Wake level) needs acknowledgement; real sends from the demo use at most the
  urgent level, said where the level is chosen.
- A visitor pastes someone else's ntfy topic: it's a public service - the demo shows topics are public,
  as the docs already do.
- Tour steps referring to controls that don't exist in the current settings (e.g. GPS off already):
  the step adapts or is skipped.
- Reduced-motion and keyboard users: the tour works without animation and with the keyboard.
- Real sending with the 10× clock: rate limit still applies in real time.

## Requirements *(mandatory)*

### Functional Requirements

**Tour**

- **FR-001**: The demo MUST offer a guided tour of 6-10 steps covering: what SQMeter is; live readings;
  the safety verdict and its reasons; triggering Rain and seeing the verdict, the alert bell and Alpaca
  agree; changing a setting and seeing the effect; the Demo panel; where to go next.
- **FR-002**: Steps that ask for an action MUST wait for the emulated device's reaction, not a timer.
- **FR-003**: The tour MUST be skippable at any step, restartable from the Demo panel, shown
  automatically only on a visitor's first visit, and usable by keyboard, on phones and with reduced motion.
- **FR-004**: The tour MUST use the app's existing look (no new visual style).

**Real notifications**

- **FR-005**: Real sending MUST be off by default and turned on explicitly by the visitor, with a short
  confirmation stating where messages will go and that keys stay in this tab.
- **FR-006**: When on, alerts and tests MUST be delivered from the visitor's browser to Pushover and ntfy;
  MQTT only to brokers offering secure WebSockets; webhooks only where the target allows browser requests.
- **FR-007**: The message content (title, message, level, sound, stacked events) MUST be what the device
  would send - built by the device's own code (spec 016) - mapped to each service as the firmware does.
- **FR-008**: Each delivery's result MUST be shown per channel in the alert list, with the service's
  error in plain words when it fails.
- **FR-009**: Real deliveries MUST be rate-limited per tab (default: at most 1 per channel per 30 s and
  10 per channel per tab session).
- **FR-010**: Keys and topics entered for real sending MUST live only in memory for the tab - not in
  saved demo state, URLs or logs - and be cleared by Reset demo.
- **FR-011**: The demo's security policy MUST allow only the services the visitor enabled, and only
  while real sending is on.

### Key Entities

- **Tour step**: target control, text, required action (optional), completion condition.
- **Real-sending session**: on/off, enabled channels with credentials (memory only), rate-limit counters.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A first-time visitor finishes the tour in under 3 minutes.
- **SC-002**: With an ntfy topic configured, a triggered Rain alert reaches a subscribed phone within 10 s.
- **SC-003**: With real sending off, 0 outbound requests (spec 016's network test still passes).
- **SC-004**: With real sending on, 100% of outbound requests go to services the visitor configured.
- **SC-005**: Keys entered for real sending are absent from storage after the tab closes (verified by test).

## Assumptions

- Service capabilities as checked on 2026-10-08 (Clarifications). If a service stops accepting browser
  requests, its option is shown as unavailable rather than failing silently.
- The tour is for the demo build only; the device's own UI doesn't get it.
- Rate-limit defaults may be tuned in the plan.

## Dependencies

- Spec 016 (the emulated device; its no-outbound guarantee is relaxed only under FR-005/FR-011).
- Spec 008 (alert content and channel mappings).
