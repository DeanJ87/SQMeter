---
description: "As-built task list (backfill) for Dashboard, Demo and Screenshots"
---

# Tasks: Dashboard, Demo and Screenshots

**Input**: Design documents from `specs/010-dashboard/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 Demo build with MSW mocks in web/src/mocks/ and docs workflow in .github/workflows/docs.yml

## Phase 2: Foundational

- [X] T002 Live streams and card visibility rules in web/src/components/Dashboard.tsx

## Phase 3: User Story 1 - Everything at a glance (P1)

- [X] T003 [US1] Cards for each sensor, safety, sun & moon, device in web/src/components/Dashboard.tsx
- [X] T004 [US1] Alerts bell in web/src/components/AlertsBell.tsx

## Phase 4: User Story 2 - My layout (P2)

- [X] T005 [US2] Masonry layout and arrange mode in web/src/components/Masonry.tsx

## Phase 5: User Story 3 - Try it and read about it (P2)

- [X] T006 [US3] Screenshot suite in web/tests/screenshots.spec.ts

## Phase 6: Convergence

- [X] T007 Add a dashboard guide to the docs (cards, visibility rules, Arrange, Sun & Moon, the bell) and embed the generated screenshots — docs/assets/screenshots/*.png are produced on every deploy but no page uses them — per FR-004 / SC-001 (missing)
- [X] T008 Fix the mock /ws/status uptime (mockStatus.uptime + Date.now()/1000 shows ~20,700 days in the demo and screenshots) in web/src/mocks/handlers.ts per FR-005 / SC-002 (contradicts)
- [X] T009 Push mock /ws/status every 2 s like the device (mock uses 5 s) in web/src/mocks/handlers.ts per FR-005 (contradicts)
- [X] T010 Extend web/tests/screenshots.spec.ts to the Alpaca page, each Settings tab and the alerts flyout per FR-004 / SC-001 (partial)
