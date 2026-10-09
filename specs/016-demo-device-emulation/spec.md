# Feature Specification: Demo That Behaves Like the Device

**Feature Branch**: `feat/demo-emulation` (to be created when implementation starts)

**Created**: 2026-10-08

**Status**: Implemented (PR #79)

**Input**: User description: "Demo that mirrors the real device. The live demo must behave like a real SQMeter: every page and link works under the demo's sub-path (today the Alpaca page's links go to GitHub 404s, and its live IsSafe says true while the Safety Monitor card says Unsafe because the mocks are independent canned responses). The demo should be a stateful emulated device: changing settings changes behaviour. State lives only in the browser - never a backend. Anything that would reach the internet on a real device is simulated in the browser and never makes real outbound requests, so the public demo can't be used to send traffic anywhere or be abused. The demo must be kept in sync with the device's API contracts, ideally checked automatically. Long term the docs and demo should move to their own domain."

## Clarifications

### Session 2026-10-08

- Q: How long should demo changes last? → A: Until the browser tab closes (a refresh keeps them; each new visit starts from defaults), plus a "Reset demo" button.
- Q: Own domain - which, and when? → A: Later. Build the demo and docs so they work at any domain and path now; move once a domain is chosen (candidates: a project domain such as sqmeter.space, or self-hosted under sqmeter.dean0.space with /demo).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Everything in the demo works (Priority: P1)

A visitor clicks through every page of the public demo - including the Alpaca page's links, setup
pages and API URLs - and nothing leads to an error page or shows information that contradicts
itself.

**Why this priority**: Broken links and contradictions make the project look broken; today the
Alpaca page's links are 404s and its live IsSafe disagrees with the Safety Monitor card.

**Independent Test**: Crawl every page and every in-app link of the published demo; zero error
pages, and safety shown on the dashboard, the Alpaca page and the Alpaca live state agree.

**Acceptance Scenarios**:

1. **Given** the published demo, **When** a visitor follows any link the UI shows (description,
   configured devices, setup pages, API base, device state, IsSafe), **Then** it opens a working
   demo view or response inside the demo, never the hosting site's error page.
2. **Given** the emulated device is unsafe, **When** the visitor views the dashboard, the Alpaca
   page and the Alpaca live state, **Then** all say unsafe, for the same reasons.
3. **Given** the demo is hosted under a sub-path (or later its own domain), **When** pages and links
   are opened, **Then** they resolve within the demo.

---

### User Story 2 - Settings change the emulated device (Priority: P1)

A visitor changes settings and sees the effect, as on a real device: GPS off removes the GPS
card and GPS time; a location makes Sun & Moon and the darkness readout use it; switching a sensor
off removes it from readings and the safety verdict; safety rules change the verdict; alerts on/off,
alert levels and MQTT publish switches change what "would be sent".

**Why this priority**: The demo is how people evaluate the device; a demo that ignores its own
settings misrepresents it.

**Independent Test**: For each setting group, change a value, save, and check the dependent pages
and API responses change the way the device's would.

**Acceptance Scenarios**:

1. **Given** GPS on, **When** the visitor turns GPS off and saves, **Then** the GPS card and GPS
   readings disappear and time shows as NTP-sourced.
2. **Given** a location is entered, **When** saved, **Then** Sun & Moon, the darkness readout and the
   device's reported sun altitude use that location.
3. **Given** a safety rule (e.g. maximum cloud cover) is tightened past the current reading, **When**
   saved, **Then** the verdict turns unsafe with that reason everywhere it's shown, after the same
   delays as the device.
4. **Given** the rain sensor is switched off, **When** saved, **Then** rain disappears from readings,
   the dashboard and Alpaca, and the rain rule can't make the verdict unsafe.
5. **Given** a change that requires a restart on the device, **When** saved, **Then** the demo asks
   for the restart and applies it after a simulated restart.

---

### User Story 3 - The demo is safe to publish (Priority: P1)

Nothing a visitor does in the demo leaves their browser: no update download, no alert send, no
MQTT or webhook call, no time or name lookups, no WiFi join, no upload goes anywhere.

**Why this priority**: The demo is public. It must not become a way to send traffic, spam a
notification service, or touch any device or service.

**Independent Test**: Exercise every action in the demo (send tests, update checks and installs,
uploads, WiFi setup, restarts) with network monitoring; the only requests are for the demo's own
files.

**Acceptance Scenarios**:

1. **Given** the demo, **When** a visitor presses Send test, checks for updates, installs an update,
   uploads a file, joins WiFi or restarts, **Then** the outcome is simulated and shown as on the
   device, and no request leaves the browser other than loading the demo itself.
2. **Given** a visitor enters their own Pushover keys, webhook URL or MQTT broker, **When** they test
   it, **Then** nothing is contacted and the demo says it's a simulation.
3. **Given** any state a visitor creates, **When** they leave, **Then** it exists only in their browser.

---

### User Story 4 - The demo can't drift from the device (Priority: P2)

When the device's API changes (as the readings and MQTT formats did in v0.2.0-beta.2), the demo
either changes with it or the build fails.

**Why this priority**: The demo has silently drifted before (wrong uptime, wrong versions,
independent mock answers).

**Independent Test**: Change a field in the device's documented response format without updating
the demo; the automated checks fail.

**Acceptance Scenarios**:

1. **Given** the documented response formats (readings, status, safety, settings, Alpaca), **When**
   the demo's responses are checked against them automatically, **Then** any difference fails the build.

---

### User Story 5 - Make things happen (Priority: P2)

A visitor can see the device react to weather: rain starting, clouds rolling in, a sensor failing,
dawn - with the verdict, alerts list, bell and Alpaca state all responding.

**Why this priority**: Shows the safety and alert features, which are the device's main value,
without waiting for real weather.

**Independent Test**: Trigger each scenario and check the verdict, reasons, alert list and Alpaca
state change as the device's would.

**Acceptance Scenarios**:

1. **Given** the demo, **When** the visitor triggers "rain", **Then** the readings show rain, the verdict
   turns unsafe with "Rain detected", a "Rain detected" alert appears at its configured level, and
   after the rain clear delay (shortened for the demo, and labelled as such) it clears.

---

### User Story 6 - Docs and demo on their own domain (Priority: P3)

The documentation and demo move from `deanj87.github.io/SQMeter` to the project's own domain,
with old links redirecting.

**Why this priority**: Long-term goal; nothing is blocked on it.

**Independent Test**: Open the docs and demo on the new domain; old `deanj87.github.io/SQMeter/...`
links land on the matching new page.

**Acceptance Scenarios**:

1. **Given** the new domain, **When** a visitor opens the docs or demo, **Then** they work with HTTPS,
   and the demo works at its new path without changes to its links (US1-AC3).
2. **Given** an old link, **When** opened, **Then** it redirects to the same page on the new domain.

---

### Edge Cases

- Private browsing or storage disabled: the demo still works, starting from defaults each time.
- A visitor sets values the device would reject: the demo rejects them with the device's messages
  (same validation).
- Two tabs open: each behaves consistently; changes made in one aren't required to appear live in
  the other.
- A visitor wants to start over: a visible "Reset demo" returns the emulated device to defaults.
- Firmware or web-UI upload in the demo: accepted and "installed" by simulation, without reading
  more than needed of the file and without keeping it.
- Time: the demo uses the browser's clock; its "device time" is always valid and labelled as such.
- Links meant for Alpaca clients (API base, discovery): shown as on the device, opening a demo view
  of the response rather than a dead link.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Every page and every link the web UI shows MUST work in the demo when hosted under a
  sub-path or its own domain; no link may lead outside the demo except to the project's docs and
  repository.
- **FR-002**: The demo MUST behave as one emulated device: readings, status, safety verdict and
  reasons, alerts, Alpaca state and device state MUST be derived from the same emulated state, so
  they never contradict each other.
- **FR-003**: Saved settings MUST change the emulated device's behaviour the way they change the
  real device's (sensors and GPS on/off, location, safety rules and delays, alert settings and
  on/off, MQTT publish groups, Alpaca on/off, units, hostname/mDNS), including which changes need a
  restart.
- **FR-004**: The emulated device MUST use the device's own rules for derived values and decisions
  (sky quality, cloud cover, safety verdict, alert triggering and stacking, darkness), not
  separately written approximations.
- **FR-005**: Settings validation in the demo MUST be identical to the device's.
- **FR-006**: The demo MUST NOT make any network request except for its own files; update checks,
  update installs, uploads, alert sends and tests, MQTT and webhook tests, WiFi scans and joins,
  restarts and time sync MUST be simulated and their results shown as on the device.
- **FR-007**: Where the demo simulates an action that would contact the outside world, it MUST say so
  briefly (e.g. "Demo: nothing was sent").
- **FR-008**: Emulated state MUST stay in the visitor's browser only, lasting until the
  tab closes (a refresh keeps it, a new visit starts from defaults); a visible "Reset demo" MUST restore defaults.
- **FR-009**: The demo MUST offer weather scenarios (at least: rain, clouding over, clearing,
  a sensor failing, dawn) that drive the emulated sensors, with long device delays shortened and
  labelled.
- **FR-010**: The demo's responses MUST be checked automatically against the device's documented
  response formats (readings, status, safety, settings, Alpaca management and device state) on
  every change; a mismatch MUST fail the build.
- **FR-011**: Alpaca URLs shown in the demo (description, configured devices, setup, API base, device
  state, IsSafe) MUST open the emulated response or page within the demo.
- **FR-012**: Docs and demo screenshots MUST keep being generated from the demo (spec 010), now from
  a consistent emulated state.
- **FR-013**: The docs and demo MUST work unchanged at any domain and base path (today
  `deanj87.github.io/SQMeter` and `/SQMeter/demo/`; later the project's own domain, e.g. a root domain
  or `/demo` under a self-hosted site). Moving is a later step once a domain is chosen; old links MUST
  then redirect to the same page.

### Key Entities

- **Emulated device state**: settings, sensor presence and values, weather scenario, alerts on/off,
  alert history, safety history, uptime and clock - the single source the demo's pages and API
  answers are derived from.
- **Weather scenario**: a named, time-based change to the emulated sensors (rain, cloud, clear,
  sensor failure, dawn).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A crawl of the published demo finds 0 links leading to an error page.
- **SC-002**: In every scenario, the safety verdict shown on the dashboard, the Alpaca page and the
  Alpaca live state agree 100% of the time.
- **SC-003**: For each settings group, a visitor sees the effect of a saved change within 5 seconds
  (or after the simulated restart where the device needs one).
- **SC-004**: Network monitoring while exercising every demo action records 0 requests other than the
  demo's own files.
- **SC-005**: A deliberate mismatch between the demo's responses and the device's documented formats
  fails the automated checks.
- **SC-006**: Each weather scenario produces the same verdict, reasons and alerts as the device logic
  would for the same inputs.

## Assumptions

- The demo stays a static site in the visitor's browser (hosted on GitHub Pages or the new domain);
  there is no server side.
- "Behaves like the device" means the web UI and its API answers; Alpaca discovery (UDP), Bluetooth
  and serial aren't emulated, and real Alpaca clients can't connect to the demo.
- Firmware-only behaviours that can't run in a browser are emulated from the same rules where the
  device's logic is shared; where it isn't, the plan decides how to keep them equal (FR-004).
- Long device delays (rain clear delay, safe delay, cooldowns) may be shortened in scenarios, always
  labelled.
- The demo's sensor values stay plausible (spec 010 FR-005).

## Dependencies

- Spec 010 (dashboard, demo and screenshots) - this replaces its "plausible mock data" approach.
- Spec 013 (data interfaces) - the documented response formats the demo is checked against.
- Constitution III (testable logic in lib/): sharing the device's decision logic with the demo
  depends on that logic being portable.
