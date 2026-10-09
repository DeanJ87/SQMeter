# Tasks: Dashboard at a glance

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/inventory.md](contracts/inventory.md)

## Phase 1: Device field (US3)

- [ ] T001 `Rain::clearRemainingMs()` in lib/RainLogic (+ native tests: held, released, still raining, never rained)
- [ ] T002 `readings.rain.clearInSeconds` in lib/Readings and the device/demo readings document; readings schema, REST docs; demo core rebuilt and its `pending()` using the shared function

## Phase 2: Inventory and enforcement (US5)

- [ ] T003 [P] `web/src/dashboard/inventory.json`: shown entries for every state in the spec's table, not-shown globs with reasons for the rest (D1, D2)
- [ ] T004 [P] `tools/dashboard/check.py` + `test_check.py` (D3); quality gate rule DASH-02, build step
- [ ] T005 DASH-01/DASH-02 in docs/development/coding-standards.md; constitution quality gate references DASH-01 (FR-004)

## Phase 3: At a glance (US1, US2, US4, US6)

- [ ] T006 `web/src/dashboard/glance.ts` `glanceItems()` with priority order and healthy line (D4, D8) + Vitest for every rule
- [ ] T007 `AtAGlance.tsx` at the top of the dashboard: healthy one line; problem items with since, consequence and fix (Resume/Pause action, links); count + "show all" beyond the top three on phones (FR-005..FR-007, FR-020)
- [ ] T008 Alerts and imaging app: shared wording (D5); waiting / paused / sending / not in effect (D-12); imaging app watching / stopped checking / disconnected (FR-008..FR-010)
- [ ] T009 Settings not in effect: fetch `/api/settings/effective`, summary + list, "no channel can send" problem (FR-011, D9)
- [ ] T010 Fault-state sensor cards (FR-013, D6); greyed last values with age while disconnected (US2-4)
- [ ] T011 Safety card: rules not in effect with reasons and links; rain hold countdown (FR-012)
- [ ] T012 Device & Network: IPv6 (or "no address yet"), `.local` name, MQTT state; clock/location problem with consequence; time and location source on demand (FR-015, FR-016)
- [ ] T013 Bluetooth phone alarm problem with acknowledge; update available from the Updates page's session result (FR-017, FR-018, D10)
- [ ] T014 Demo marker and demo clock label (FR-022, D11)
- [ ] T015 Announcements only on important changes; words not colour (FR-019)

## Phase 4: Translations

- [ ] T016 Every new string in en.json with context notes; translated in all 13 languages; check.mjs and literals pass

## Phase 5: Tests, docs, budgets

- [ ] T017 `web/tests/dashboard.spec.ts`: per shown inventory entry, drive the demo into the state and assert shown, then hidden (FR-003); add the dashboard states to the a11y inventory (SC-006)
- [ ] T018 Docs: user-guide/dashboard.md describes every inventory item; screenshots via the screenshot test; REST; diagrams checked (FR-023)
- [ ] T019 Budget: UI bundle gzip delta ≤ 4 KB (SC-007); firmware delta recorded
- [ ] T020 Converge
