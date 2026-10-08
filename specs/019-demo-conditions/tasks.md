# Tasks: Demo Conditions - Set the Sensor Readings

**Input**: Design documents from `/specs/019-demo-conditions/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: requested by the spec's Success Criteria and the constitution: native for `lib/`,
Vitest for demo logic, Playwright for the panel.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

- [x] T001 Confirm the baseline: `pio test -e native -f test_alert_logic`, `cd web && npx vitest run`, and record both firmware image sizes (`pio run -e esp32dev`, `-e esp32dev-ble`) for the PR

## Phase 2: Foundational (blocking)

- [x] T002 [P] Add `WaitKind`, `Wait` and the const query `AlertEngine::waits(nowSeconds, rules)` (contracts/core-pending.md) in lib/AlertLogic/include/AlertEngine.h and lib/AlertLogic/src/AlertEngine.cpp. Keep sensor names from the last `update()` so `sensor:<name>` can be reported.
- [x] T003 [P] Native tests for `waits()` in test/test_alert_logic/test_main.cpp: startup grace, settle pending, cooldown after a notice, nothing when settled, unaffected state (the query doesn't change later `update()` results)
- [x] T004 Emulate the TSL2591 rolling average in tools/demo-core/bridge.cpp: a time-stamped lux window of `skyAveraging.windowSeconds` (clamped 10..300, reset on change); night-mode lux = mean; track the last step (>10%) for settling
- [x] T005 Add `EmulatedDevice::pending()` in tools/demo-core/bridge.cpp (skyAveraging, rainClear, alerts) and bind it; rebuild with tools/demo-core/build.sh; update SOURCE_HASH (`python3 tools/demo-core/source_hash.py --check`)
- [x] T006 [P] POSIX time-zone evaluator `offsetAt`, `toLocalParts`, `fromLocal`, `formatIsoWithOffset`, `parsePosixTz` in web/src/demo/posixTz.ts with tests in web/src/demo/__tests__/posixTz.test.ts (London, Paris, Sydney, Atacama `<-04>4<-03>,M9.1.6/24,M4.1.6/24`, UTC0, JST-9, invalid → UTC)
- [x] T007 [P] Conditions model in web/src/demo/conditions.ts: types, `DEFAULT_CONDITIONS`, `RANGES`, `setInput` (clamp + report), `setDifferential`, `startRamp`, `advance` (ramps on the device clock), `toCoreInputs(conditions, nowMs, sunLux, gps)` with natural variation unless `steady`; tests in web/src/demo/__tests__/conditions.test.ts

## Phase 3: User Story 1 - Set what each sensor reports (P1) 🎯 MVP

**Goal**: The panel's primary controls are the raw readings, and the device derives everything.

**Independent test**: clear threshold -30 in Settings; air 20, sky -12, humidity 40 → clear; sky -5 → cover rises.

- [x] T008 [US1] Rewire web/src/demo/device.ts: hold `conditions` and `ramps`; each tick advances ramps, applies device rules (rain off → rain 0), builds inputs via `toCoreInputs` (light from the sun via `skyLux` in `sun` mode); setters `setInput`, `setDifferential`, `setFault`, `setSteady`; remove scenario state
- [x] T009 [US1] Reduce web/src/demo/simulator.ts to sky light and sun helpers (`skyLux`, `simulatorLocation`, `darkestTime`, `nextSunRising`, `nextSunSetting`); remove scenarios and the fixed-difference sky model (superseded by shortcuts)
- [x] T010 [US1] Rebuild web/src/demo/DemoPanel.tsx with collapsible sensor groups (`<details>`): Sky & clouds (sky, IR ambient, differential, light mode, lux), Air (temperature, humidity, pressure), Rain (rate, lens fault), Wind (speed, gust, direction), GPS (fix), each with "Not responding". Use number inputs with units in their labels (FR-021), clamping messages (FR-004), and rain/wind/GPS groups unavailable with reason + link when switched off (FR-016)
- [x] T011 [US1] Styles for groups and inputs in web/src/index.css reusing `field`, `input` and `note` classes

## Phase 4: User Story 2 - What the device makes of it, and waits (P1)

**Independent test**: rain 2 → 0: "Rain clear delay - …" counts down, then the reason clears.

- [x] T012 [US2] Device-derived summary in DemoPanel: SQM, NELM, Bortle, cover + condition, dew point, rain state, verdict + reasons, alerts armed, all read from the core's documents (FR-005, FR-006)
- [x] T013 [US2] Waits list in DemoPanel from `pending()` plus safety `secondsUntilSafe` and active ramps, worded per the spec ("Sky brightness averages over 90 s - settled in 40 s"); hidden when nothing is pending (FR-007)

## Phase 5: User Story 3 - Shortcuts from the device's settings (P1)

**Independent test**: thresholds -30/-20 → Clear → clear; Overcast → overcast; cloud rule off → Cloud just unsafe explains.

- [x] T014 [P] [US3] Shortcuts in web/src/demo/shortcuts.ts: `clear`, `overcast`, `cloudUnsafe`, `rain`, `rainStops`, `darkSky(sqm)`, `dewRisk`, each `(config, conditions) → ShortcutResult` with `used` text, clamping notes and unreachable reasons + links (research R3); tests in web/src/demo/__tests__/shortcuts.test.ts covering default and custom thresholds, cloud rule off, faulted sensor, dew margin 0, calibration offset
- [x] T015 [US3] Shortcut buttons + ramp choice (Instant / 40 s / 2 min / 10 min; cloud default 40 s) and the result note in DemoPanel; `device.applyShortcut(result, rampMs)` (FR-008..FR-011)
- [x] T016 [US3] `?scenario=` mapping (contracts/demo-state.md) in web/src/demo/device.ts `start()` (FR-012)

## Phase 6: User Story 4 - Date, time and place (P1)

**Independent test**: North Pole + 31 Dec 23:00 + Dawn → explained, clock unchanged.

- [x] T017 [P] [US4] Presets in web/src/demo/presets.ts: time presets (now, darkest, dawn, dusk, midsummer, midwinter, 31 Dec 23:00) resolved in the device's time zone and location with unreachable reasons, and location presets (research R5); tests in web/src/demo/__tests__/presets.test.ts (London dawn sun -12..-6 rising; North Pole 31 Dec no sunrise; Sydney midsummer = December)
- [x] T018 [US4] device.ts: `setClock(at)`, `applyTimePreset`, `applyLocationPreset` (applyConfig location + ntp.timezone), local time/date for `core.tick` and `now` text from posixTz; GPS reports the device location
- [x] T019 [US4] web/src/demo/handlers.ts: `/api/status` `time.iso` in the device's zone with `+hhmm` (contracts/demo-state.md)
- [x] T020 [US4] Time & place section in DemoPanel: device date, time, zone and location (FR-017), exact date-time entry, time preset buttons, location preset buttons, result notes, link to Settings → Time & Location, 10× switch

## Phase 7: User Story 5 - Phone layout (P2)

- [x] T021 [US5] Bottom-sheet panel at ≤ 560 px (max 75vh, internal scroll, sticky head) in web/src/index.css; keep the toggle's save-bar/toast clearance (FR-020)

## Phase 8: Persistence

- [x] T022 Saved state v2 with v1 migration and Reset clearing both keys in web/src/demo/device.ts (contracts/demo-state.md, FR-018)

## Phase 9: Polish & Cross-Cutting

- [x] T023 [P] Playwright in web/tests/demo.spec.ts: update existing scenario tests to the new controls; add US1 thresholds test, US2 rain-clear wait, US3 unreachable, US4 North Pole dawn + London dawn (SC-006), SC-005 links (rain/cloud/night), SC-007 phone save buttons on every settings tab, SC-008 every control with zero outbound requests
- [x] T024 [P] Rewrite docs/live-demo.md: inputs, shortcuts, waits, time & place, links (FR-022); `mkdocs build --strict`
- [x] T025 Full verification: native tests, both firmware builds (sizes vs T001), web typecheck + Vitest, demo build + Playwright, source hash check
- [x] T026 Close out: update spec status, PR #81 body (built, tests, converge, device checks)

## Dependencies

- T002 → T005 (the bridge uses `waits()`); T004 → T005
- T006, T007 → T008; T008 → T010 → T012/T013/T015/T020
- T014 → T015 → T016; T017 → T018 → T020
- Everything → T023 → T025 → T026

## Parallel opportunities

- Within Foundational: T002+T003 (C++), T006 (TS) and T007 (TS) run together.
- Then T014 and T017 run together (separate pure modules).

## Implementation strategy

- **MVP:** Phases 1–3 (raw inputs driving the core).
- **Then:** waits (US2), shortcuts (US3), time & place (US4), phone (US5), persistence, then
  polish.

## Phase 10: Convergence

- [x] T027 Add a GPS "not responding" control: bridge treats `gps.failed` as a sensor fault, panel GPS group offers it per FR-001 (partial)
- [x] T028 Let the visitor choose the Dark sky target SQM (default 21.5) next to the shortcut per US3 / FR-008 (partial)
- [x] T029 Show the sensor-range message when the sky minus IR sensor differential is clamped per FR-004 (partial)
- [x] T030 Give location presets an elevation and report it as the simulated GPS altitude per spec Key Entities (location preset) (partial)
