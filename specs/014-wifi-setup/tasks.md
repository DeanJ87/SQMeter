---
description: "As-built task list (backfill) for WiFi Setup and Network Presence"
---

# Tasks: WiFi Setup and Network Presence

**Input**: Design documents from `specs/014-wifi-setup/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 Station mode with back-off and soft AP "SQM-Setup" with wildcard DNS in src/WiFiManager.cpp

## Phase 2: User Story 1 - Get it on my WiFi (P1)

- [X] T002 [US1] Captive-portal probe redirects in src/WebServer.cpp
- [X] T003 [US1] WiFi scan/select/password in web/src/components/settings/NetworkTab.tsx

## Phase 3: User Story 3 - Stay connected (P2)

- [X] T004 [US3] Auto-reconnect with exponential back-off in src/WiFiManager.cpp

## Phase 4: Polish & Cross-Cutting

- [X] T005 [P] First-boot guide in docs/getting-started/first-setup.md

## Phase 5: Convergence

- [X] T006 Add mDNS (`<hostname>.local`) — the firmware has none, yet the docs tell users to open http://sqmeter.local — in src/WiFiManager.cpp per FR-003 / US2-AC1 (missing)
- [X] T007 Land the captive portal on a WiFi setup screen (network list, password, Connect) instead of redirecting probes to the dashboard (/) in src/WebServer.cpp and the web UI per FR-002 / US1-AC2 (missing)
- [X] T008 Use /api/wifi/connect from the setup screen or remove it — only the demo mocks call it today — in src/WebServer.cpp per FR-005 (unrequested)
- [X] T009 Use one hostname in the docs: examples mix sqmeter.local (first-setup, configuration, alerts) and sqm-esp32.local (rg15) while the default hostname is sqm-esp32, in docs/ per FR-006 / SC-003 (contradicts)
- [X] T010 Describe the actual first-boot flow in docs/getting-started/first-setup.md (portal lands on the dashboard today; WiFi is under Settings → Network) until T007 lands, per FR-006 (contradicts)

## Phase 6: Convergence

- [X] T011 Add an mDNS on/off setting (default on) to the wifi config in src/Config.cpp and web/src/components/settings/NetworkTab.tsx, and advertise the HTTP service, per FR-003 (clarified 2026-10-08) (missing)
