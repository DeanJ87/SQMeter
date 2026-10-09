# Feature Specification: Diagrams in the Docs

**Feature Branch**: `spec/024-docs-diagrams`
**Created**: 2026-10-08
**Status**: Implemented (PR #90) - converged
**Input**: User description: "there's also an open PR and issue about mermaid docs, we should spec this in, because they could be really helpful to have in the docs"

## Context

- **PR #30**, "Add Mermaid diagrams to docs", opened by Codex in May 2026 and still open. It does two things:
  - Turns on Mermaid fences in `mkdocs.yml`.
  - Adds four diagrams:
    - System architecture on `docs/index.md`
    - Integration paths on `docs/api/integrations.md`
    - First-setup state diagram on `docs/getting-started/first-setup.md`
    - OTA slot state diagram on `docs/user-guide/ota.md`
- **PR #30 predates most of today's behaviour.** Alerts, Alpaca, the safety verdict, rain and wind, BLE, the readings schema, GitHub-release OTA, the captive portal rework and the WebAssembly demo all came later, so its diagrams are incomplete and in places no longer accurate. For example, the setup flow now joins WiFi without the reboot it shows, and the OTA diagram shows only manual upload.
- **No issue is about Mermaid.** The closest open issue is **#36**, "Visual of the hardware assembled and or assembly?". It asks for visuals of the assembled hardware. A wiring/assembly diagram (DIA-13 below) answers part of it; photos and video belong to the hardware repo and are out of scope.
- **The docs site** is sqmeter.dev, built with MkDocs Material using the single dark (`slate`) scheme, themed like the app.
  - Today it has no diagrams.
  - Most behaviour is explained only in prose and tables.
  - The state machines (safety delay, rain latch, alert cooldown, arming) are the hardest parts to understand from prose.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Understand how SQMeter fits together at a glance (Priority: P1)

A new user, or someone deciding whether to build one, opens the docs home page. Before reading any prose they see one diagram:
- the sensors on the left;
- the ESP32 in the middle;
- the ways data leaves on the right: dashboard, REST/WebSocket, MQTT/Home Assistant, Alpaca/N.I.N.A., alerts (ntfy, Pushover, webhook, MQTT, Bluetooth).

**Why this priority**: It's the most-read page and the question asked most ("what does it connect to?"). One correct diagram answers it.

**Independent Test**: Open the docs home page on desktop and on a phone. The architecture diagram renders in the site's colours, every box names something that exists in the current firmware, and the caption describes the diagram in words.

**Acceptance Scenarios**:

1. **Given** the docs home page, **When** it loads, **Then** a system architecture diagram shows every sensor the firmware supports, the device, and every output path. Each optional part (GPS, RG-15, anemometer, BLE) is marked optional.
2. **Given** a screen reader user, **When** they reach the diagram, **Then** a text alternative describes the same connections in words.
3. **Given** a phone-width screen, **When** the diagram is wider than the screen, **Then** it scrolls or scales inside its own box, and the page itself never scrolls sideways.

---

### User Story 2 - Understand why the verdict is unsafe and when it will turn safe (Priority: P1)

An observatory owner whose roof stayed shut reads the safety page and sees two diagrams:
- a **decision flow**: which rules are evaluated, in what order, which fail safe on missing data, and that rain is checked even when other data is stale;
- a **state machine**: unsafe → safe-delay countdown → safe, and what restarts the countdown.

From these they can tell why the verdict was unsafe and how long it will stay that way.

**Why this priority**: Safety behaviour is the product's core promise (Constitution Principle I). Misreading it costs equipment.

**Independent Test**: Put the diagram next to the safety evaluation code and its tests. Every branch and transition in the diagram corresponds to code, and no code path that changes the verdict is missing.

**Acceptance Scenarios**:

1. **Given** the Alpaca/safety page, **When** a reader follows the decision flow, **Then** they can tell which rule produced each reason the device reports (rain, cloud cover, SQM, wind, humidity, sensor stale or missing, and so on).
2. **Given** the safe-delay state machine, **When** the verdict flips from unsafe to safe, **Then** the diagram shows the delay, what resets it, and that the raw verdict and the reported verdict differ during the delay.
3. **Given** the rain page, **When** a reader looks at the rain latch diagram, **Then** it shows rain detected → latched → the clear delay after the last drop → clear, and that the clear delay is what holds the verdict unsafe.

---

### User Story 3 - Understand alerts end to end (Priority: P2)

A user setting up alerts sees the alert lifecycle as a diagram:
- an event is detected;
- it is checked against arming/schedule and cooldown;
- it is formatted;
- it is delivered on each enabled channel;
- recovery events follow.

They understand why an alert did or didn't arrive.

**Why this priority**: "Why didn't I get an alert?" is the most common alert question, and the answer is a path through several gates.

**Independent Test**: For each gate in the diagram, a test or code path in the alert engine and dispatcher implements it, and nothing that can suppress an alert is left out.

**Acceptance Scenarios**:

1. **Given** the alerts page, **When** a reader follows the diagram, **Then** every reason an alert can be suppressed is shown: master off, not armed or schedule, cooldown, event off, channel off or inactive.
2. **Given** spec 021 (alert schedule wording, PR #84) changes the arming model, **When** that ships, **Then** this diagram is updated in the same change (FR-009).

---

### User Story 4 - Follow a process step by step: setup, update, integration (Priority: P2)

A user follows first setup, an OTA update or an N.I.N.A. connection with a sequence or flow diagram:
- **Setup:** joining the SQM-Setup hotspot, the captive portal opening, choosing WiFi, the device joining, finding it on the network.
- **OTA:** check releases, download, verify, write the inactive slot, reboot, roll back on failure.
- **N.I.N.A.:** discovery, connect, polling IsSafe and the weather properties.

**Why this priority**: These flows are where users get stuck, and a sequence shows who does what.

**Independent Test**: Walk through each flow on the spare device and confirm each step happens in the order shown.

**Acceptance Scenarios**:

1. **Given** the first-setup page, **When** a user follows the diagram on a phone, **Then** each step matches what they see, including the portal opening by itself and what to do when it doesn't.
2. **Given** the OTA page, **When** a reader looks at the update diagram, **Then** both paths are shown (the GitHub release update and manual upload), as are the firmware and filesystem steps and the rollback on a failed boot.
3. **Given** the Alpaca page, **When** a reader looks at the N.I.N.A. diagram, **Then** it shows discovery (UDP 32227), the management API and the device API calls N.I.N.A. makes, and where the setup page link goes.

---

### User Story 5 - Contributors see how the code is organised (Priority: P3)

A contributor sees diagrams of:
- the code layers: hardware-free `lib/` logic, `src/` hardware and network, web UI, demo;
- the demo architecture: the firmware's logic compiled to WebAssembly, served through the browser's request interception;
- the Spec Kit workflow: specify → clarify → plan → tasks → implement → converge.

**Why this priority**: Useful, but contributors can read the code. User-facing diagrams come first.

**Independent Test**: Each component in the layer diagram maps to a directory that exists, and the dependency arrows match the rule "`lib/` never includes hardware headers" (spec 017 STRUCT-01).

**Acceptance Scenarios**:

1. **Given** the development docs, **When** a contributor reads the architecture diagram, **Then** every box names a real directory or module, and arrows only point in allowed dependency directions.
2. **Given** the live demo page, **When** a reader looks at the demo diagram, **Then** it shows that the demo runs the device's own logic in the browser and that nothing leaves the browser (spec 016).

---

### Edge Cases

- **Code changes but the diagram doesn't.** Each diagram names the source files it reflects, and a check flags it when those files change (FR-007). The pull request author then updates the diagram or confirms it is still accurate.
- **Invalid Mermaid syntax.** The docs build fails instead of publishing a broken diagram or raw code (FR-006).
- **JavaScript disabled or still loading.** The text alternative is still readable (FR-005). A raw code block is not acceptable as the only fallback.
- **Phone width.** A wide flowchart scrolls inside its own container. Dense diagrams prefer a top-to-bottom layout so they stay readable at about 400 px.
- **Print and PDF export.** Diagrams print legibly, or the caption carries the meaning.
- **A future light scheme.** The site is dark-only today. If a light scheme is added, diagrams follow it, so colours are never hard-coded inside a diagram (FR-003).
- **GitHub.** GitHub also renders Mermaid in Markdown, so the same fenced diagrams render on GitHub in the docs folder, READMEs and spec files, in GitHub's own colours. That is acceptable.
- **Unsupported diagram types.** A diagram type that neither the site's renderer nor GitHub supports is not used.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The docs site MUST render diagrams written as text (Mermaid) inside the Markdown pages. They are reviewed in pull requests like the prose. Image files are not used for diagrams that can be expressed as text.
- **FR-002**: Diagrams MUST be readable at about 400 px wide. Any diagram wider than the content column scrolls within its own container, and the page never scrolls horizontally.
- **FR-003**: Diagrams MUST use the site's theme colours and fonts. Colours come from the theme, not from styling inside each diagram, so a theme change restyles every diagram. A diagram may use the safe/unsafe/warning colours only alongside a text label, never colour alone (spec 022).
- **FR-004**: The docs site MUST NOT load the diagram renderer from a third-party CDN at runtime. The renderer is a pinned version served from sqmeter.dev itself, so the docs work without access to other hosts and nothing is fetched from elsewhere.
- **FR-005**: Every diagram MUST have a caption and a text alternative that conveys the same information (steps, states, connections) in words. The text alternative is available to screen readers and readable without JavaScript.
- **FR-006**: The docs build in CI MUST fail when any diagram has invalid syntax or fails to render, naming the page and diagram.
- **FR-007**: Every diagram MUST name the source files or modules it reflects. A check in CI and convergence MUST report each diagram whose named sources changed since the diagram was last confirmed accurate. Confirming is a deliberate step, such as updating a recorded fingerprint next to the diagram. The check reports without blocking for ordinary changes, and blocks for changes to safety-verdict code (Constitution Principle I).
- **FR-008**: Diagrams MUST describe the current behaviour of the code they cite. Anything optional or configurable is labelled so. Names in diagrams (states, reasons, events, topics, endpoints) match the names the device and UI use.
- **FR-009**: A pull request that changes behaviour shown in a diagram MUST update that diagram (Constitution Principle VII, "Docs move with behaviour"). The coding standard (spec 017) gains a rule to this effect.
- **FR-010**: The diagram catalogue below MUST be delivered in priority order. P1 diagrams are required for this feature to be complete; P2 and P3 can ship in later increments.
- **FR-011**: PR #30 is reconciled as follows:
  - Its renderer setup is reused, with the CDN replaced per FR-004.
  - Its four diagrams are redrawn against the current code: architecture, integration paths, first setup, OTA.
  - The implementation pull request credits PR #30 and states that it supersedes it. PR #30 is closed only when that lands, not as part of this spec.
- **FR-012**: The system architecture diagram (DIA-01) MUST be the same picture wherever it appears: the docs home page and the README (GitHub renders it). It is defined in one place, or the convergence check (FR-007) treats the two copies as one diagram.

### Diagram catalogue (priority order)

| ID | Priority | Diagram | Type | Page | Reflects |
|---|---|---|---|---|---|
| DIA-01 | P1 | System architecture: sensors (TSL2591, MLX90614, BME280; optional GPS, RG-15, anemometer and vane), the ESP32 (logic, settings storage, web UI files), and outputs (dashboard, REST/WebSocket, MQTT and Home Assistant, Alpaca/N.I.N.A., alerts: ntfy, Pushover, webhook, MQTT, BLE) | flowchart | `index.md`, README | `src/main.cpp`, `src/sensors/`, `src/WebServer.cpp`, `src/MQTTClient.cpp`, `src/AlertDispatcher.cpp`, `src/BleService.cpp` |
| DIA-02 | P1 | Safety verdict decision flow: each rule, fail-safe on missing or stale data, rain checked regardless of other data, reasons produced | flowchart | `user-guide/alpaca.md` (safety rules) | `lib/AlpacaLogic` (SafetyEvaluator), `lib/DeviceCore` |
| DIA-03 | P1 | Safe-delay state machine: unsafe → waiting (countdown) → safe, and what resets it; raw versus reported verdict | state | `user-guide/alpaca.md` | `lib/AlpacaLogic`, `lib/DeviceCore`, `lib/SafetyHistoryLogic` |
| DIA-04 | P1 | Rain latch: dry → raining → latched (clear delay after the last drop) → dry; daily reset; sensor fault | state | `hardware/rg15.md`, `user-guide/alpaca.md` | `lib/RainLogic` |
| DIA-05 | P1 | Readings pipeline: raw sensor values → derived values (SQM with averaging, NELM, Bortle, cloud cover from the sky-minus-air temperature difference with humidity correction, dew point) → one readings document → REST, WebSocket, MQTT and Alpaca | flowchart | `reference/sky-quality.md`, `api/integrations.md` | `lib/SkyLogic`, `lib/Readings`, `lib/DeviceCore` |
| DIA-06 | P2 | Alert lifecycle: detect → gates (master, arming/schedule, event enabled, cooldown) → format (default or custom wording) → channels → delivery result → recent list | flowchart | `user-guide/alerts.md` | `lib/AlertLogic`, `src/AlertDispatcher.cpp` |
| DIA-07 | P2 | First setup and captive portal: SQM-Setup hotspot → portal opens (or open the address) → choose network → join → find the device | sequence | `getting-started/first-setup.md` | `src/WiFiManager.cpp`, `lib/CaptiveDns` |
| DIA-08 | P2 | OTA update: check releases → choose → download firmware and filesystem → write the inactive slot → reboot → roll back on a failed boot; manual upload path | sequence | `user-guide/ota.md` | `src/OtaUpdater.cpp`, `lib/ReleaseLogic` |
| DIA-09 | P2 | MQTT topic map: `<base>/state`, `safe`, `safety`, `availability`, `alerts`, `alerts/armed`, `alerts/armed/set`, `diagnostics`, Home Assistant discovery; retained or not; direction | flowchart | `user-guide/mqtt.md` | `src/MQTTClient.cpp`, `lib/Readings` |
| DIA-10 | P2 | Alpaca with N.I.N.A.: discovery (UDP 32227) → management API → connect → poll IsSafe and weather properties → setup page link | sequence | `user-guide/alpaca.md` | `lib/AlpacaLogic`, `src/WebServer.cpp` |
| DIA-11 | P3 | Code layers and dependency direction: `lib/` (hardware-free logic) ← `src/` (hardware, network) ← web UI; demo reuses `lib/` | flowchart | `development/workflow.md` or a new architecture page | directory structure, spec 017 STRUCT rules |
| DIA-12 | P3 | Demo architecture: device logic compiled for the browser, the sky simulator feeding it, request interception answering the UI, nothing outbound | flowchart | `live-demo.md` | `tools/demo-core`, `web/src/demo` |
| DIA-13 | P3 | Wiring/assembly overview: which sensor connects to which bus and pins (I²C 21/22, GPS UART, RG-15 UART, anemometer and vane inputs), and power. Answers part of issue #36. | flowchart | `hardware/overview.md` or `hardware/assembly.md` | `include/` pin definitions, hardware docs |
| DIA-14 | P3 | Development workflow: specify → clarify → plan → tasks → implement → converge → PR | flowchart | `development/workflow.md` | `.specify/`, constitution |
| DIA-15 | P3 | Integration paths (redrawn from PR #30): REST pull, WebSocket live, MQTT publish, Alpaca, alerts, and who uses each | flowchart | `api/integrations.md` | `src/WebServer.cpp`, `src/MQTTClient.cpp` |

### Key Entities

- **Diagram**: a text diagram in a docs page, with an ID, caption, text alternative, source references and a confirmation fingerprint.
- **Source reference**: a file or module a diagram reflects. Changes to it make the diagram "needs review".
- **Confirmation fingerprint**: a record that a person checked the diagram against the code at a given state of its sources.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All five P1 diagrams (DIA-01 to DIA-05) are published on sqmeter.dev and render on desktop and at 400 px width with no horizontal page scroll.
- **SC-002**: 100% of diagrams have a caption, a text alternative and source references. A check enforces this, and a deliberately incomplete diagram fails it.
- **SC-003**: A deliberately broken diagram fails the docs build, and the failure names the page.
- **SC-004**: Changing a file referenced by a diagram makes the check list that diagram as needing review. Doing so for safety-verdict code blocks the merge until the diagram is confirmed.
- **SC-005**: Loading any docs page with a diagram makes no request to a host other than sqmeter.dev, the existing font host excepted.
- **SC-006**: A reviewer comparing each P1 diagram with the code finds no state, rule, transition or name that differs from the code.
- **SC-007**: The docs home page answers "what does SQMeter connect to?" without reading prose. A first-time reader can list the outputs after looking at the diagram alone.

## Assumptions

- **Mermaid** is the diagram language. MkDocs Material renders it natively, GitHub renders it in Markdown, and PR #30 already uses it. PlantUML and image-based tools are not used.
- **Self-hosting the renderer.** Material loads Mermaid from a public CDN by default. Serving a pinned copy from the site is assumed possible by supplying the library to the theme. The plan confirms the mechanism and its size; the docs site has no size budget like the device.
- **Dark only.** The docs site stays dark-only (`slate`) for now. "Matching the theme" means the site's dark palette and fonts, with light support coming free through FR-003 if a light scheme is added.
- **Diagrams are docs-site only.** They are not added to the device's web UI, whose flash budget is tight. The UI can link to the docs.
- **Text alternatives** are written by hand next to each diagram, as a collapsible "Diagram in words" or a caption plus list, not generated.
- **The confirmation fingerprint** is a hash of the referenced sources recorded beside the diagram. The plan may choose a simpler equivalent, as long as SC-004 holds.
- **Specs 019–023** are open PRs not yet on main. Where they change behaviour shown here (spec 021 alert arming, spec 020 effective settings), the affected diagrams are drawn to whatever is on main when this is implemented and updated with those features (FR-009).
- **Issue #36.** No open issue is about Mermaid specifically. #36 (hardware visuals) is the nearest, and DIA-13 addresses its "how does it go together" part; photos and video remain with the SQMeter-Hardware repo.
