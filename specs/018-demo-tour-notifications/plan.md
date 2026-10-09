# Implementation Plan: Demo Tour and Real Notifications

**Branch**: `feat/018-demo-tour` | **Date**: 2026-10-09 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/018-demo-tour-notifications/spec.md`

## Summary

Two demo-only additions (`web/src/demo/`, never in the device UI bundle):

- **Guided tour** (US1): a small step engine (`tour/steps.ts`, `tour/Tour.tsx`) that outlines real
  controls, waits for the emulated device's reaction on action steps (FR-002), offers "Do it for me",
  and is offered once per browser (localStorage flag). Non-modal, keyboard and reduced-motion friendly,
  docked at the bottom on phones. Restartable from the Demo panel.
- **Real notifications** (US2, US3): an opt-in "Real notifications" section in the Demo panel. Credentials
  live only in a JS memory object (`realSend.ts`). When the emulated device records an alert, the device's
  own code builds each service request (new pure `lib/DeviceCore` `AlertDelivery`, also used by the
  firmware's `AlertDispatcher`, FR-007) and the browser sends it to ntfy.sh, Pushover or an
  MQTT-over-secure-WebSocket broker (tiny MQTT 3.1.1 publisher, no dependency). Results show per channel
  in the alert list (FR-008); a per-channel rate limit (1 per 30 s, 10 per tab) applies (FR-009).

## Technical Context

**Language/Version**: TypeScript 5 strict (Preact + Vite), C++17 (`lib/`, firmware, Emscripten demo core).

**Primary Dependencies**: existing only (MSW, Playwright, Vitest, Unity, ArduinoJson 6, Emscripten 6). No
MQTT library: the publisher is ~80 lines over `WebSocket`.

**Testing**: Unity (`test/test_device_core`: delivery requests), Vitest (tour engine, rate limit, MQTT
packet encoding, real-send gating), Playwright (tour with "Do it for me", keyboard/Esc, phone width, axe;
real sending with `page.route` fulfilling ntfy/Pushover so nothing really leaves CI; the default
no-outbound test unchanged; storage holds no keys).

**Constraints**: spec 016 FR-006 (no outbound by default); the demo CSP is a static `<meta>` (GitHub Pages
can't send headers), see research R1; firmware behaviour unchanged (identical requests).

## Constitution Check

| Principle | Status |
|---|---|
| I Fail-safe verdict | Not touched |
| II Alpaca | Not touched |
| III Testable pure logic | Request building moves into `lib/DeviceCore` (native-tested), shared by firmware and demo |
| IV Budgets | Firmware: refactor only; device UI bundle unchanged (demo-only code) |
| V Quiet UI | Tour reuses `card`, `btn`, `note`; one offer, dismissable |
| VI Trusted-LAN security | Keys memory-only (FR-010); CSP adds exactly two HTTPS hosts (R1) |
| VII Docs | `docs/live-demo.md`, DIA-12 |
| VIII Code quality | `tools/quality/check.py`: no new findings |

## Project Structure

```text
lib/DeviceCore/include/AlertDelivery.h, src/AlertDelivery.cpp   # shared request building
src/AlertDispatcher.cpp                                       # uses AlertDelivery
test/test_device_core/test_main.cpp                           # delivery tests
tools/demo-core/bridge.cpp                                    # deliveryRequests(), requested mask
web/src/demo/realSend.ts, mqttPublish.ts                      # memory-only session, sending
web/src/demo/panel/RealNotifications.tsx                      # opt-in UI
web/src/demo/tour/{steps.ts,tour.ts,Tour.tsx}                 # tour
web/src/demo/handlers.ts                                      # real results into /api/alerts/recent
web/vite.demo.config.ts                                       # CSP
web/tests/demo-tour.spec.ts, demo-real-send.spec.ts
docs/live-demo.md
```
