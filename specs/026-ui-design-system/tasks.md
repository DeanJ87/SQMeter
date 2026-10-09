# Tasks: UI design system and copy

**Input**: [spec.md](spec.md), [plan.md](plan.md), [audit.md](audit.md)

## Phase 1: Rules

- [ ] T001 Add the "UI (DS)" section (DS-01..DS-10, DS-20..DS-27) to docs/development/coding-standards.md and reference it from the quality gate in .specify/memory/constitution.md (FR-011)
- [ ] T002 Add a "Design (mockup)" section to .specify/templates/spec-template.md requiring cited DS rules and a reviewed mockup for UI changes (FR-015)

## Phase 2: US1 Status card (P1)

- [ ] T003 [US1] Create web/src/dashboard/StatusCard.tsx: tiles (Safety, Alerts, Data, Imaging app), problem rows (name + pill + "?", whole row links or acts), card pill "All good" / "{n} to check" (FR-001..003, plan D1-D6)
- [ ] T004 [US1] Short labels for every glance item in web/src/dashboard/glance.ts (sensor names only, state in a pill, consequence in detail)
- [ ] T005 [US1] Mount the Status card first in web/src/components/dashboard/order.ts and cards.tsx; remove AtAGlance.tsx and the strip CSS in web/src/index.css
- [ ] T006 [US1] Move the demo marker to the Demo panel toggle (web/src/demo/DemoPanel.tsx)
- [ ] T007 [US1] Update web/src/dashboard/inventory.json locations (glance → card:status) and web/tests/dashboard.spec.ts, web/src/dashboard/__tests__ to assert the card (FR-010)

## Phase 3: US2 Language card (P1)

- [ ] T008 [US2] In-card progress in web/src/components/settings/LanguageCard.tsx: status row with pill, ProgressMeter, one-line outcome Note with Retry and "?" (FR-004)
- [ ] T009 [US2] Remove LanguageProgressBanner and LanguageNotice from web/src/components/Layout.tsx; delete web/src/components/LanguageProgress.tsx; Status card row for a missing/failed language
- [ ] T010 [US2] Update web/tests/language-progress.spec.ts: progress in the card, header unchanged

## Phase 4: US3 Long values and explanations (P2)

- [ ] T011 [US3] Device & Network rows in web/src/dashboard/DeviceCard.tsx (FR-005)
- [ ] T012 [US3] Sun & Moon hint in web/src/components/SunMoonCard.tsx (FR-006)
- [ ] T013 [US3] Audit A9-A15: language hint, Alerts tab status rows, phone note, illuminance formatting (format.ts), Alpaca imaging-app pills, Updates downgrade one-liner + "?"

## Phase 5: US4 Copy (P2)

- [ ] T014 [US4] Rewrite the audit's English strings and any the copy check flags; update en.context.json types (FR-008)
- [ ] T015 [US4] Regenerate all 13 locales from the new English with the glossaries; `translate.py --all --record`; i18n checks pass
- [ ] T016 [US4] Copy check tools/quality/copy_check.py + test, wired into tools/quality/checks.py (FR-012)

## Phase 6: US5 Drift (P2)

- [ ] T017 [US5] One name per sensor (glossary) across dashboard, System, Alpaca and alerts; label check tools/quality/label_check.py + test (FR-013)
- [ ] T018 [US5] Layout check in web/tests/layout-overlap.spec.ts: nothing between header and cards at 1280 and 390 px (FR-014)
- [ ] T019 [US5] Docs: docs/user-guide/dashboard.md, languages.md, coding-standards DASH section, screenshots (FR-009)
- [ ] T020 Supersession notes in specs/023 and specs/025 (already added with the spec; verify)

## Phase 7: Verify

- [ ] T021 Gates: native tests, firmware builds, web tsc/vitest/build/build:demo, Playwright, quality, UI size, dashboard inventory, diagrams, route registry, mkdocs --strict
- [ ] T022 Visual check of every changed view at 1280 and 390 px against the mockups and DS rules
- [ ] T023 Spare-device check (flash, every page, dashboard and Language card screenshots)
