# Tasks: Demo Tour and Real Notifications

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md),
[contracts/](contracts/)

**Tests**: required by the spec (SC-003, SC-004, SC-005) and the constitution.

## Phase 1: Foundational - requests built by the device's code (FR-007)

- [ ] T001 New `lib/DeviceCore/include/AlertDelivery.h` + `src/AlertDelivery.cpp`: `urlEncode`, `fullTitle`, `ntfyTags(AlertType)`, `pushoverRequest(alert, pushover creds, sound default, deviceName)`, `ntfyRequest(alert, server, topic, token, deviceName)`, `alertJson(alert, deviceName, epoch)` (MQTT + webhook payload, stacked events) returning `HttpRequest{url, contentType, headers, body}` / strings
- [ ] T002 `src/AlertDispatcher.cpp` uses `AlertDelivery` for MQTT, Pushover, ntfy and webhook (same bytes as before)
- [ ] T003 [P] Native tests in `test/test_device_core/test_main.cpp`: Pushover body (encoding, priority, emergency retry/expire, sound fallback), ntfy URL/headers/token, tags for every type, stacked events in JSON
- [ ] T004 `tools/demo-core/bridge.cpp`: `Record.requested` mask; `deliveryRequests(recordId, credentialsJson)` per [contracts/delivery-requests.md](contracts/delivery-requests.md) (Wake clamped to Urgent, per-channel tests only to that channel); `realTestAlert()` records a test alert for the Demo panel; rebuild `web/src/demo/core/*`, update SOURCE_HASH

## Phase 2: User Story 3 - Safe to offer (P1)

- [ ] T005 [US3] `web/vite.demo.config.ts` CSP: `connect-src 'self' ws: wss: https://ntfy.sh https://api.pushover.net` (research R1)
- [ ] T006 [US3] `web/src/demo/realSend.ts`: memory-only session (FR-010), validation, rate limit 1/30 s and 10/tab per channel on wall time (FR-009), gate: no outbound unless enabled + channel set up; `reset()` used by Reset demo
- [ ] T007 [P] [US3] Vitest `web/src/demo/__tests__/realSend.test.ts`: off → no fetch; rate limits; validation; reset clears; Wake clamp comes from the core (contract)

## Phase 3: User Story 2 - Alerts on my phone (P2)

- [ ] T008 [US2] `web/src/demo/mqttPublish.ts`: MQTT 3.1.1 CONNECT/CONNACK/PUBLISH/DISCONNECT over `wss://` with subprotocol `mqtt` (R6); [P] Vitest packet encoding in `web/src/demo/__tests__/mqttPublish.test.ts`
- [ ] T009 [US2] Delivery loop: after each device step/test, new records → `deliveryRequests` → fetch/publish; results with plain-words errors (Pushover/ntfy JSON `errors[0]`/`error`) (FR-008)
- [ ] T010 [US2] `web/src/demo/handlers.ts`: `/api/alerts/recent` channels replaced by real results for records sent for real ("Delivered", failure text, rate-limit skip)
- [ ] T011 [US2] `web/src/demo/panel/RealNotifications.tsx` in the Demo panel: off by default, confirmation listing where messages go and that keys stay in this tab (FR-005); ntfy topic (+ token), Pushover keys, MQTT wss URL/topic/user/password; "Send a test"; Wake→Urgent note; webhook and self-hosted ntfy shown as unavailable with the reason (US2-4)
- [ ] T012 [US2] Playwright `web/tests/demo-real-send.spec.ts`: requests fulfilled by `page.route` (nothing really leaves); ntfy test → one POST to ntfy.sh with device headers + list shows the result; Pushover error text; second send within 30 s skipped; nothing outbound while off; after reload no key in local/sessionStorage (SC-003/004/005)

## Phase 4: User Story 1 - Guided tour (P1)

- [ ] T013 [US1] `web/src/demo/tour/steps.ts` (8-9 steps per FR-001; action steps with `done` predicates on device state and `doIt`; rain step adapts when the rain sensor is off) and `tour.ts` (start/stop/next, offered-once flag in localStorage)
- [ ] T014 [US1] `web/src/demo/tour/Tour.tsx`: non-modal card in the app's style, outline box, focus to heading, Esc/Skip end, "Step n of N", phone docking, reduced motion (FR-003/004); offer card on first visit; mount in demo `main.tsx`; "Take the tour" button in `DemoPanel.tsx`
- [ ] T015 [P] [US1] Vitest `web/src/demo/__tests__/tour.test.ts`: offered once, dismiss/finish stored, action step waits for `done`
- [ ] T016 [US1] Playwright `web/tests/demo-tour.spec.ts` (clean storage): offer → Start → "Do it for me" on rain → verdict unsafe → step advances → finish; Esc ends; restart from panel; phone width every step's card in the viewport; axe on the tour card; keyboard only
- [ ] T017 [US1] `web/playwright.config.ts`: preload `sqm.demo.tour.v1=dismissed` so other suites aren't interrupted

## Phase 5: Polish

- [ ] T018 `docs/live-demo.md`: the tour and Real notifications (what's allowed, keys stay in the tab, limits, webhook/self-hosted unavailable); "Nothing leaves your browser" amended; DIA-12 demo diagram updated if affected (`python3 tools/docs/diagrams.py`)
- [ ] T019 Verify: native tests, both firmware builds, web tsc/vitest/demo build/Playwright, SOURCE_HASH, `tools/quality/check.py` no new findings, mkdocs --strict
