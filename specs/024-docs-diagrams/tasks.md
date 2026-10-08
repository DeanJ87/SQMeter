# Tasks: Diagrams in the Docs (024)

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/diagram-block.md](contracts/diagram-block.md), [quickstart.md](quickstart.md)

**Tests**: the spec requires checks (FR-006, FR-007, SC-002–SC-004), so the check tooling gets unit tests and failure proofs.

## Phase 1: Setup

- [x] T001 Pin `mkdocs-material==9.7.7` in docs/requirements.txt, and install from it in .github/workflows/docs.yml
- [x] T002 [P] Add `mermaid@11.17.2` as a web devDependency, plus the `docs:vendor` and `docs:diagrams` scripts, in web/package.json and web/package-lock.json
- [x] T003 [P] Ignore the vendored file `docs/assets/javascripts/vendor/` in .gitignore

## Phase 2: Foundational (blocking all stories)

- [x] T004 Enable the `mermaid` custom fence (reused from PR #30) and `theme.custom_dir: overrides` in mkdocs.yml
- [x] T005 Load `assets/javascripts/vendor/mermaid.min.js` before Material's bundle, only on pages containing `class="mermaid"`, in overrides/main.html (research R2)
- [x] T006 [P] Write web/scripts/vendor-mermaid.mjs: copy `node_modules/mermaid/dist/mermaid.min.js` to docs/assets/javascripts/vendor/
- [x] T007 [P] Style the diagrams in docs/stylesheets/sqmeter.css:
  - `.md-typeset .mermaid` scrolls horizontally inside its box;
  - `figure.diagram` and its caption get styles;
  - Material's mermaid font variable gets the site font.
- [x] T008 Write tools/docs/diagrams.py (data-model.md):
  - parse diagram blocks in `docs/**/*.md` and README.md;
  - check completeness: metadata, accTitle/accDescr, caption, words block, no inline styling;
  - compute fingerprints over SourceRefs (file, directory, `#symbol`);
  - report ok / stale / stale-blocking / bad-source / mismatch, with exit codes;
  - `--confirm ID|all` rewrites fingerprints;
  - `--site DIR` checks for the vendored script and for CDN references;
  - emit GitHub annotations under `GITHUB_ACTIONS`.
- [x] T009 [P] Write tools/docs/test_diagrams.py covering: block parsing, symbol extraction (definition vs declaration, nested braces), fingerprint stability, each error state, `--confirm` round trip, and the site check
- [x] T010 Write web/scripts/check-diagrams.mjs:
  - extract the mermaid blocks;
  - launch Chromium (`channel: 'chrome'` on CI);
  - load the vendored mermaid;
  - `parse` and `render` each block;
  - fail naming `file:line DIA-NN`.
- [x] T011 Add .github/workflows/diagrams.yml: run `python3 tools/docs/diagrams.py` and the unit tests on pull requests touching src/, include/, lib/, docs/, README.md, web/src/demo/, tools/ and the workflow
- [x] T012 Add to .github/workflows/docs.yml, around `mkdocs build --strict`: `npm run docs:vendor`, `npm run docs:diagrams` and `python3 tools/docs/diagrams.py --site site`. Add `tools/docs/**`, `overrides/**` and `README.md` to its path filters

## Phase 3: User Story 1: SQMeter at a glance (P1) 🎯 MVP

**Goal**: The docs home page and the README show the system architecture.
**Independent test**:
- The home page renders DIA-01 at desktop width and at 400 px.
- Every box names a real component.
- The README copy is identical.

- [x] T013 [US1] Draw DIA-01 (system architecture) in docs/index.md, replacing PR #30's version. It reflects src/main.cpp, src/sensors/, src/WebServer.cpp, src/MQTTClient.cpp, src/AlertDispatcher.cpp and src/BleService.cpp, with optional parts labelled
- [x] T014 [US1] Add the same DIA-01 to README.md, with a `*Figure:*` caption and a `<details>` "Diagram in words" (FR-012)

## Phase 4: User Story 2: Why unsafe, and when safe (P1)

**Goal**: The safety rules, the safe delay and the rain latch are diagrams.
**Independent test**:
- Every branch and transition maps to `evaluateSafety`, `SafeDelayFilter` and `Rain::observe`/`expire`.
- The diagrams are blocking.

- [x] T015 [US2] Draw DIA-02 (safety verdict decision flow) in docs/user-guide/alpaca.md. It shows:
  - the rule order;
  - rain and wind evaluated regardless of freshness;
  - no data / stale / sensor fault;
  - fresh-data-only threshold rules, including the faulted-sensor skips;
  - the reasons, and what IsSafe returns when Alpaca is off.

  Mark it `blocking: true`.
- [x] T016 [US2] Draw DIA-03 (safe-delay state machine: raw vs reported, the countdown, what resets it, the boot start) in docs/user-guide/alpaca.md. Mark it `blocking: true`
- [x] T017 [US2] Draw DIA-04 (rain latch: dry → raining → held → dry; daily reset affects only the total; stale/lens fault → rain-sensor reason) in docs/hardware/rg15.md, and link it from docs/user-guide/alpaca.md. Mark it `blocking: true`
- [x] T018 [US2] Draw DIA-05 (readings pipeline: raw → averaged light → calibrated SQM/NELM/Bortle; sky minus air temperature, humidity corrected → cloud; dew point → one readings document → REST, WebSocket, MQTT, Alpaca) in docs/reference/sky-quality.md

## Phase 5: User Story 3: Alerts end to end (P2)

- [x] T019 [US3] Draw DIA-06 (alert lifecycle with every gate: startup grace, rule off, night-only, settling, cooldown, event level Off, switched off (not imaging), master switch, channel off, WiFi/OTA/TLS skips, and the Wake → Bluetooth path) in docs/user-guide/alerts.md

## Phase 6: User Story 4: Setup, update, integration (P2)

- [x] T020 [P] [US4] Draw DIA-07 (first setup sequence, including the portal probes, join-then-save, the restart after ~15 s, and the timeout path) in docs/getting-started/first-setup.md, replacing PR #30's version
- [x] T021 [P] [US4] Draw DIA-08 (OTA sequence: GitHub release path, filesystem first then firmware, verify before the boot switch, manual upload path; no automatic rollback of a crashing image) in docs/user-guide/ota.md. Correct the overstated rollback sentence (research R6.2)
- [x] T022 [P] [US4] Draw DIA-09 (MQTT topic map with retain flags and directions, plus Home Assistant discovery) in docs/user-guide/mqtt.md
- [x] T023 [P] [US4] Draw DIA-10 (Alpaca with N.I.N.A.: UDP 32227 discovery, management API, connect, polling, the setup redirect) in docs/user-guide/alpaca.md
- [x] T024 [P] [US4] Draw DIA-15 (integration paths, redrawn from PR #30) in docs/api/integrations.md

## Phase 7: User Story 5: Contributors (P3)

- [x] T025 [P] [US5] Draw DIA-11 (code layers and dependency direction) in docs/development/workflow.md
- [x] T026 [P] [US5] Draw DIA-12 (demo architecture: WASM device core, simulator, MSW, the 404.html device URLs, nothing outbound) in docs/live-demo.md
- [x] T027 [P] [US5] Draw DIA-13 (wiring overview: buses, default pins, power; addresses part of issue #36) in docs/getting-started/hardware.md, and link it from docs/hardware/overview.md
- [x] T028 [P] [US5] Draw DIA-14 (Spec Kit workflow) in docs/development/workflow.md

## Phase 8: Polish & cross-cutting

- [x] T029 Add the FR-009 rule ("behaviour shown in a diagram changes → update and re-confirm it") to CONTRIBUTING.md and docs/development/contributing.md, with how-to steps from quickstart.md
- [x] T030 Confirm every diagram's fingerprint (`diagrams.py --confirm all`) after reviewing each against its sources
- [x] T031 Run every check:
  - mkdocs build --strict;
  - diagrams.py;
  - its unit tests;
  - docs:diagrams;
  - `--site`.

  Prove each failure mode from quickstart.md, then revert.
- [x] T032 Render review: serve site/ on port 4185 and screenshot every diagram page at 1280 px and 400 px in headless Chromium. Check the palette, scroll containment and captions, and confirm there are no requests to third-party hosts (SC-005)
- [x] T033 Update PR #90: title, body crediting PR #30 and stating it supersedes it, and the research R6 findings

## Dependencies

- Phase 1 → Phase 2 → the stories.
- The stories are independent of each other.
- T030–T032 come after all diagrams are drawn.
- Diagram tasks marked [P] touch different files.

## Phase 9: Convergence

- [x] T034 [SC-005] Override Material's repo link (overrides/partials/source.html) without `data-md-component="source"`, so no docs page fetches repository facts from api.github.com. Keep the GitHub link and name. Prove with the render review that a diagram page makes no third-party request
- [x] T035 [US2/T015] Show on DIA-02 (docs/user-guide/alpaca.md) that Alpaca IsSafe is false with a NotConnected error while Alpaca is off, in the diagram and its words, and link to DIA-10
- [ ] T036 [FR-009] When spec 017's docs/development/coding-standards.md is on main, add the diagram rule to it as a numbered rule (DOC-01: behaviour shown in a diagram is updated and re-confirmed in the same PR) and reference it from CONTRIBUTING.md
