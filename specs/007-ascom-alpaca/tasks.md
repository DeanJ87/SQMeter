---
description: "As-built task list (backfill) for ASCOM Alpaca Devices"
---

# Tasks: ASCOM Alpaca Devices

**Input**: Design documents from `specs/007-ascom-alpaca/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 Alpaca protocol helpers and discovery parsing in lib/AlpacaLogic/src/AlpacaProtocol.cpp and lib/AlpacaLogic/src/AlpacaDiscovery.cpp

## Phase 2: Foundational

- [X] T002 Router for management + device APIs in lib/AlpacaLogic/src/AlpacaRouter.cpp with tests

## Phase 3: User Story 1 - N.I.N.A. finds and uses the device (P1)

- [X] T003 [US1] UDP discovery listener in src/WebServer.cpp (handleAlpacaDiscovery)
- [X] T004 [US1] MAC-based UniqueIDs in lib/AlpacaLogic/src/AlpacaProtocol.cpp

## Phase 4: User Story 2 - Conformance (P1)

- [X] T005 [US2] GET/PUT parameter casing, 400s, Platform 7 methods in lib/AlpacaLogic/src/AlpacaRouter.cpp
- [X] T006 [US2] Simulator and ConformU workflow in tools/alpaca-sim/ and .github/workflows/alpaca-conformance.yml

## Phase 5: User Story 3 - Weather data per property (P2)

- [X] T007 [US3] Per-sensor property mapping, sensor descriptions, time since update in lib/AlpacaLogic/src/ObservingConditionsMapper.cpp

## Phase 6: User Story 4 - Setup and inspection (P3)

- [X] T008 [US4] /setup redirect in src/WebServer.cpp; section→tab mapping in web/src/components/settings/tabs.ts
- [X] T009 [US4] Alpaca page in web/src/components/Alpaca.tsx

## Phase 7: Polish & Cross-Cutting

- [X] T010 [P] Alpaca guide in docs/user-guide/alpaca.md

## Phase 8: Convergence

- [X] T011 Answer Alpaca discovery from the network task (AsyncUDP) instead of polling once per main-loop pass, in src/WebServer.cpp (handleAlpacaDiscovery), per FR-001 / SC-002 — measured 0.4–1.2 s on 2026-10-07 (partial)
- [X] T012 Update docs/user-guide/alpaca.md "Enabling Alpaca support", "Setup button" and "If discovery doesn't find the device" to Settings → Safety → ASCOM Alpaca and the "Serve Alpaca devices" toggle, and mention the restart prompt, per FR-007 / SC-004 (contradicts)
