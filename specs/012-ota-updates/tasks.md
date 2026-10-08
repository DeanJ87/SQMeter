---
description: "As-built task list (backfill) for Firmware and Web UI Updates"
---

# Tasks: Firmware and Web UI Updates

**Input**: Design documents from `specs/012-ota-updates/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 Release workflow with version injection and assets in .github/workflows/build.yml

## Phase 2: Foundational

- [X] T002 OtaUpdater release fetch with pinned roots and TlsLock in src/OtaUpdater.cpp

## Phase 3: User Story 1 - One-click update (P1)

- [X] T003 [US1] Track/asset selection incl. BLE prefix in src/OtaUpdater.cpp (parseGithubReleases)
- [X] T004 [US1] Updates page with track, release and install in web/src/components/Updates.tsx

## Phase 4: User Story 2 - Manual install (P2)

- [X] T005 [US2] /api/update and /api/update/fs routes in src/WebServer.cpp

## Phase 5: Polish & Cross-Cutting

- [X] T006 [P] docs/user-guide/ota.md

## Phase 6: Convergence

- [X] T007 Investigate and fix manual firmware uploads failing with "Could not activate partition" on the first attempt and succeeding on retry (seen on every flash 2026-10-04…08) in src/WebServer.cpp (/api/update) per FR-003 / SC-001 (contradicts)
- [X] T008 Give local builds a version that orders correctly against releases (include/version.h says 0.0.2 while releases are 0.2.x, so every release looks newer) — e.g. bump with each release plus a dev suffix — per FR-005 / SC-002 (contradicts)
- [X] T009 Move release-list parsing (track filter, asset matching for standard/BLE) out of src/OtaUpdater.cpp into lib/ with native tests per FR-006 (missing)
- [X] T010 Return non-2xx on failed uploads (see 013 T019) per FR-004 (contradicts)
- [X] T011 Update docs/user-guide/ota.md to the Updates page's labels (Firmware / Release track / Manual upload / image type) — addressed by PR #73 — per FR-007 (contradicts)
