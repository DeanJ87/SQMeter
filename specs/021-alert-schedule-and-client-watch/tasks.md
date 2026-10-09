# Tasks: Alert schedule wording and "imaging app lost" alerts

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: required by the spec (FR-015, FR-017, SC-002, SC-004, SC-005) and the constitution.

## Phase 1: Setup

- [x] T001 Create `test/test_alert_schedule/test_main.cpp` (Unity suite skeleton) for the schedule state machine

## Phase 2: Foundational (blocks every story)

- [x] T002 Add per-device activity counters (`requests`, `disconnects`, last `ClientID`) and `activity()`/`resetConnections()` to `lib/AlpacaLogic/include/AlpacaRouter.h` and `lib/AlpacaLogic/src/AlpacaRouter.cpp`, without changing any response
- [x] T003 Add `alerts.sendMode` ("any"/"whileConnected"), `clientSilentSafetySeconds` (120, 30–3600), `clientSilentWeatherSeconds` (600, 30–3600) and events `client_lost` (3), `client_back` (1), `client_disconnected` (0) to `lib/ConfigModel/include/Config.h` and defaults/validation/serialization in `lib/ConfigModel/src/ConfigModel.cpp`; `armWithAlpaca` written as `sendMode == whileConnected`, mapped on input when `sendMode` is absent
- [x] T004 Split persisted alerts JSON: client events + silence times under a second part (`Config::alertsToJson(redact, AlertsJsonPart)`), each part ≤ 3900 bytes; load both in `src/ConfigStore.cpp` (NVS key `alertclient`)
- [x] T005 [P] Native config tests in `test/test_config_model/test_main.cpp`: defaults, migration of `armWithAlpaca` true/false, `sendMode` wins, `armWithAlpaca` written back, range validation, every event at max template length fits both parts

## Phase 3: User Story 1 - Know when the imaging app stops watching (P1)

**Goal**: `client_lost` / `client_back` / `client_disconnected` alerts from shared logic.
**Independent test**: native tests; demo "Go silent" produces the alert.

- [x] T006 [US1] New `lib/AlpacaLogic/include/ClientWatch.h` + `src/ClientWatch.cpp`: per-device connected/watching/silent/lastRequest/clientId/disconnectedNow from Router activity, silence times and uptime; reset when Alpaca is disabled; re-watch after a clean disconnect only on connect
- [x] T007 [US1] Client events in `lib/AlertLogic/include/AlertEngine.h` / `src/AlertEngine.cpp`: `AlertType::ClientLost/ClientBack/ClientDisconnected`, `ClientInputs clients[2]` in `AlertInputs`, rules `onClientLost/Back/Disconnected`; tracker per device with cooldown; disconnect resets silently; exempt from startup grace and night-only
- [x] T008 [US1] `lib/DeviceCore`: `eventSettingFor` for the 3 events, `alertRules`, `addClientInputs()` (device names, `{silent_for}`, `{last_checked}`, `{client_id}`), sample alerts for tests, `writeClientWatch()` for `/api/status` `alpaca`
- [x] T009 [P] [US1] Native tests in `test/test_alpaca_logic/test_main.cpp` (router counters, ClientWatch: silence, recovery, clean disconnect, restart case, two clients, poll-without-connect, disabled) and `test/test_alert_logic/test_main.cpp` (events, cooldown/flapping, disconnect suppresses back, default wording has no product name)
- [x] T010 [US1] Firmware wiring in `src/WebServer.cpp` / `include/WebServer.h`: `ClientWatch` updated each alert pass from `alpacaRouter.activity()`, client inputs added, events dispatched before any resulting pause; `/api/status` `alpaca` section
- [x] T011 [US1] Demo core wiring in `tools/demo-core/bridge.cpp`: same as T010; `restart()` forgets connections; rebuild `web/src/demo/core/*` and update SOURCE_HASH
- [x] T012 [US1] Web: `AlertEventKey` + config/status types in `web/src/types/index.ts`, defaults in `web/src/components/settings/defaults.ts`, zod in `web/src/validation/configSchema.ts`, mocks in `web/src/mocks/data.ts`; event rows "The imaging app stops checking / is back / disconnects" with Silent-for fields and an "Alpaca is off" note in `web/src/components/settings/AlertsTab.tsx`
- [x] T013 [US1] Alpaca page client state ("Safety monitor: connected, last checked 3 s ago") in `web/src/components/Alpaca.tsx`, with a Vitest test

## Phase 4: User Story 2 - Understand when alerts are sent (P1)

**Goal**: "When to send" mode, status line with reason and time, Pause/Resume.
**Independent test**: wording test + status line test; demo disconnect pauses with reason.

- [x] T014 [US2] New `lib/AlertLogic/include/AlertSchedule.h` + `src/AlertSchedule.cpp`: send mode, pause state with reason/since, commands, connection edges, mode changes, restore/migration
- [x] T015 [P] [US2] Native tests in `test/test_alert_schedule/test_main.cpp` for every transition in data-model.md, silence never pausing, restore/migrated
- [x] T016 [US2] `lib/DeviceCore`: `writeAlertSchedule()` (the `/api/alerts/armed` object: armed, armWithAlpaca, mode, reason, since, sinceAgeMs) and reason/source names
- [x] T017 [US2] Firmware: replace `alertsArmed`/`pendingArm` with `AlertSchedule` in `src/WebServer.cpp`/`include/WebServer.h`; NVS `armed`+`reason`+`since`; REST `source=ui`; MQTT source; `/api/alerts/armed` and `/api/status` `alerts`; schedule edge applied after the pass's alerts are dispatched; "Alerts resumed" quiet notification
- [x] T018 [US2] Demo core: `setArmed(on, source)`, `armedDocument()`, `statusParts()` `alerts`, saveState/loadState of reason/since in `tools/demo-core/bridge.cpp`; `web/src/demo/{device.ts,handlers.ts}` pass `source`, include `alerts`/`alpaca` in the status document
- [x] T019 [US2] AlertsTab: replace "When you're not imaging" with **When to send** (`SelectInput`, two options), status sentence from `/api/alerts/armed` (or status), **Pause alerts**/**Resume alerts** button with `source=ui`, using the exact Wording table
- [x] T020 [P] [US2] Vitest wording test over the rendered Alerts card (no "?" labels, no negations, no product names in labels, status sentence) and status-line cases in `web/src/components/__tests__/AlertSchedule.test.tsx`
- [x] T021 [US2] Bell and history wording: `web/src/components/AlertsBell.tsx` (Pause/Resume, paused note), `web/src/components/SafetyCard.tsx` ("Alerts resumed"/"Alerts paused"); update their tests

## Phase 5: User Story 3 - Settings and integrations keep working (P2)

- [x] T022 [US3] Contract schemas: `specs/016-demo-device-emulation/contracts/schemas/status.schema.json` (`alerts`, `alpaca`) and `config.schema.json` (new alerts fields and events); `specs/013-data-interfaces/contracts/rest-responses.md` updated
- [x] T023 [US3] Native test of the 4 upgrade combinations (armWithAlpaca × armed) through Config migration + `AlertSchedule::restore` in `test/test_alert_schedule/test_main.cpp`

## Phase 6: User Story 4 - Try it in the demo (P3)

- [x] T024 [US4] `web/src/demo/DemoImagingApp.tsx`: connect / go silent / resume checking / disconnect, polling SafetyMonitor every 3 s and ObservingConditions every 60 s of demo time through the emulated Alpaca API; hosted by one line in `web/src/demo/DemoPanel.tsx`
- [x] T025 [US4] Playwright test in `web/tests/demo.spec.ts`: connect, go silent with 10×, "stopped checking" under the bell; resume → "is back"; whileConnected + disconnect → paused with reason

## Phase 7: Polish

- [x] T026 [P] Docs: `docs/user-guide/alerts.md` (model, modes, pause/resume, silence vs disconnect, 3 events, variables), `docs/user-guide/alpaca.md` (client watch), `docs/user-guide/mqtt.md` (armed = sending), `docs/api/rest.md`, `docs/user-guide/configuration.md`; `mkdocs build --strict`
- [x] T027 Update spec Wording table for the snake_case variables (research D11)
- [x] T028 Full verification: native tests, both firmware builds (flash delta), web typecheck/tests, demo build + Playwright, `tools/demo-core/source_hash.py --check`, contract tests

## Dependencies

- T002–T004 before US1/US2. T006 → T007 → T008 → T010/T011. T014 → T016 → T017/T018.
- Web tasks (T012, T019–T021) need the types (T012 first).
- T024/T025 need T011 and T018.

## Parallel opportunities

- T005, T009, T015, T020, T026 are test/doc files independent of each other.
- Firmware (T010, T017) and demo (T011, T018) wiring can proceed in parallel once lib/ is done.

## MVP

US1 (client lost alerts) + US2 (wording) together: both P1, and US2's schedule replaces the code
US1's disconnect alert must run before.

## Phase 8: Convergence

- [x] T029 [US2] Edge case "the imaging app goes silent while alerts are paused by hand - the status line still shows the client as silent": the Alerts card shows a note when `status.alpaca.clients.*.silent` ("The imaging app has gone quiet - safety monitor last checked 4 min ago.") in `web/src/components/settings/AlertsTab.tsx`, with a test in `web/src/components/__tests__/AlertSchedule.test.tsx`
- [x] T030 [US3] FR-021 "schemas are updated": add `specs/016-demo-device-emulation/contracts/schemas/alerts-armed.schema.json` for `GET /api/alerts/armed`, check it in `web/src/demo/__tests__/contracts.test.ts` and add the endpoint to `tools/contract-check.py`


## On-device verification

- [x] T031 On-device checks on the spare device (standard build, main ba66d72), 2026-10-09. All pass:
  - OTA from v0.2.0-beta.3 moved "On while N.I.N.A. is connected" to "Only while an imaging app is connected", paused (`reason: migrated`).
  - A silent client raised "Imaging app stopped checking" after 2 min, and "is back" on its return. "Back" is held by the shared cooldown; a follow-up fix is open.
  - A clean disconnect paused alerts without "stopped checking".
  - `POST /api/alerts/disarm` gave "Paused by a script".
  - Settings and the pause survived a restart.
  - ConformU SafetyMonitor: no errors.
  - Heap and stack stayed healthy.

  Left to the user: MQTT `alerts/armed` and the Home Assistant switch, which need the broker login.
