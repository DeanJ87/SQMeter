# Research: Demo Tour and Real Notifications

## R1 - Content Security Policy (FR-011)

**Decision**: `connect-src 'self' ws: wss: https://ntfy.sh https://api.pushover.net`. Real sending is gated
in code: the only code that opens outbound connections is `realSend.ts`, and it refuses unless the visitor
turned real sending on, in this tab, for that channel.

**Rationale**: The demo is on GitHub Pages, which can't send response headers, so the CSP is a `<meta>`
tag fixed at page load. A page can only *tighten* its policy at runtime (a second policy intersects),
never loosen it, so "allow only while real sending is on" (FR-011) can't be expressed in the CSP itself.
The CSP is the outer fence (two named HTTPS hosts) and the opt-in gate the inner one. `ws: wss:` was
already present (MSW's WebSocket interception of `/ws/*`) and is what lets a visitor reach their own
MQTT-over-secure-WebSocket broker; the publisher accepts `wss://` URLs only.

**Deviation from FR-006 (webhooks, self-hosted ntfy)**: allowing them would need `connect-src https:` (any
host). That widens the public page far more than the feature is worth, so in the demo they are shown as
unavailable *before* the visitor tries (US2 scenario 4).

**Alternatives**: `https:` in connect-src (rejected, above); a relay server (rejected: US3 "never through
any server of ours"); a service-worker fetch to escape the page CSP (rejected: defeats the policy).

## R2 - Requests built by the device's code (FR-007)

**Decision**: Move the pure parts of `src/AlertDispatcher.cpp` (URL encoding, "Device: Title", ntfy tags
and priority, Pushover form body, MQTT/webhook JSON) into `lib/DeviceCore` `AlertDelivery`. The firmware
sends the same bytes as before; the demo core exposes `deliveryRequests(recordId, credentials)`.

**Rationale**: the only way to guarantee "mapped to each service as the firmware does" is to run the
same code.

## R3 - Wake level

Pushover emergency (priority 2) needs acknowledgement, so real sends from the demo clamp Wake to Urgent
before building the request; the Real notifications section says so.

## R4 - Which alerts go out

Every alert the emulated device records while real sending is on goes to each channel the visitor set up
in the Demo panel (not the device's channel switches, which would need the visitor's keys twice). A
per-channel device test (`channel=pushover`) only goes to that channel. The section's "Send a test"
records a test alert through the device core.

## R5 - Credentials (FR-010)

A module-level object; never in the device config, sessionStorage, URLs or logs. Reset demo and a reload
clear it ("kept only until you reload or close this tab").

## R6 - MQTT from a browser

MQTT 3.1.1 over WebSocket (subprotocol `mqtt`): CONNECT (clean session, optional user/password), wait for
CONNACK, PUBLISH QoS 0 to `<base>/alerts` (as the firmware), DISCONNECT. Failing or closing before CONNACK
reports "This broker didn't accept a browser connection - it needs MQTT over secure WebSockets". No library.

## R7 - Rate limit (FR-009)

Per channel, wall clock (not the 10x device clock): at most one delivery per 30 s and 10 per tab session.
A limited delivery shows as skipped with the reason.

## R8 - Tour engine

- Offered once per browser: `localStorage['sqm.demo.tour.v1'] = 'done' | 'dismissed'` (per-viewer
  convenience; storage failures just mean it's offered again). Playwright's base config preloads
  `dismissed` so other suites aren't interrupted; tour tests use a clean storage state.
- Non-modal card (`role="dialog"`, `aria-modal="false"`), focus moves to the step heading, Esc and Skip
  end it; the target is outlined with a CSS box (no transition under reduced motion); on narrow screens
  the card docks to the bottom.
- Action steps complete on the device's reaction (a predicate over device state, re-checked on every
  device tick), with "Do it for me". Steps adapt: with the rain sensor off, the "make it unsafe" step uses
  the Cloud just unsafe shortcut instead.

## R9 - Where the opt-in lives

The spec mentions "the alert settings"; the feature is demo-only and Settings is being reworked by spec
020 concurrently, so the opt-in is a "Real notifications" section in the Demo panel.
