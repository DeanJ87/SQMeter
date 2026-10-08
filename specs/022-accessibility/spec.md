# Feature Specification: Accessibility Audit and Remediation

**Feature Branch**: `spec/022-accessibility` (implementation on its own branch)

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "SDD plan for an A11Y audit. An accessibility audit and remediation of the SQMeter web UI (served from the ESP32 and the demo) and the docs site (sqmeter.dev), to WCAG 2.2 AA. Cover automated checks on every page in CI with a baseline that burns down; a manual checklist (keyboard only, screen readers, zoom and reflow, reduced motion, colour not the only signal, night use); live values that don't flood screen readers; charts with text alternatives; accessible names for icon buttons; focus management in dialogs; touch target sizes. The device's storage is limited, so fixes must not bloat the UI. Publish an accessibility statement. Add accessibility rules to the coding standard."

## Survey: likely problems (audit starting scope)

A first pass over the UI code (`web/src/components`, `web/src/index.css`) found these candidate failures. The audit (US1) confirms or rejects each and adds whatever else it finds; this list is the minimum it covers.

| # | Where | Likely problem | WCAG 2.2 |
|---|---|---|---|
| A1 | `--dim` text colour (`#566675`), used for text in about 15 places | 3.1:1 on the panel background; normal text needs 4.5:1 | 1.4.3 |
| A2 | `--line` borders (`#1b2a38`) on inputs, toggles and cards | 1.3:1; the edges of form controls need 3:1 | 1.4.11 |
| A3 | `.input:focus` and `.info-tip:focus` set `outline: none` and only recolour the border | Focus indicator too faint / not guaranteed | 2.4.7, 2.4.11 |
| A4 | `InfoTip` "?" | Its `aria-label="More information"` hides the tip's text from screen readers (the tooltip isn't referenced by `aria-describedby`); it can't be dismissed with Escape; on touch it depends on focus; target is 20 px | 1.3.1, 1.4.13, 4.1.2, 2.5.8 |
| A5 | Settings tabs (`role="tablist"`) | No arrow-key navigation or roving tabindex; the tab panel isn't labelled by its tab | 2.1.1, 4.1.2 |
| A6 | Alerts flyout (`role="dialog"`) | Focus isn't moved into it on open or returned to the bell on close; no `aria-modal`/non-modal pattern decided | 2.4.3 |
| A7 | Live values (`/ws/sensors` every ~1 s, `/ws/status` every 2 s) | Nothing is announced today, including a safe → unsafe change; making cards live regions naively would announce every second | 4.1.3 |
| A8 | Sparklines (`aria-hidden`) and the Night chart (`role="img"` with a generic label) | No text alternative for the trend or for darkness and moon times; the Sun & Moon card's text covers some of it | 1.1.1 |
| A9 | Sensor status dots (`aria-hidden`) | Check that every dot comes with text; where it doesn't, colour is the only signal | 1.4.1 |
| A10 | Animations and transitions (9 in the CSS), the Masonry drag | No `prefers-reduced-motion` handling | 2.3.3 (AAA, adopted as a project rule), 2.2.2 |
| A11 | Layout | No skip link to the main content; the top navigation repeats on every page | 2.4.1 |
| A12 | Small controls (`btn-sm`, info tips, toast close, demo panel) | Targets may be under 24 × 24 CSS px | 2.5.8 |
| A13 | Demo panel (floating "✦ Demo") | It overlaps page content and the Save button on phones; check focus order and reflow at 320 px | 1.4.10, 2.4.11 |
| A14 | Forms: validation errors from the device's messages | Check that errors are tied to their field (`aria-describedby`/`aria-invalid`) and that they get announced | 3.3.1, 3.3.3 |
| A15 | Docs site (custom slate theme in `docs/stylesheets/sqmeter.css`) | The custom colours haven't been checked for contrast; neither have the screenshots' alt text or the tables | 1.4.3, 1.1.1 |

There is no separate red "night vision" theme in the UI today; the one dark theme is also the night theme. FR-010 covers what happens if one is added.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Audit: a known list of what's wrong (Priority: P1)

A maintainer can see, for every page of the device UI and the docs, which WCAG 2.2 AA criteria pass or fail, with each failure tied to a component and a fix.

**Why this priority**: You can't fix what you haven't measured. Everything else is driven by the audit's findings.

**Independent Test**: Read the audit report and check that it covers every page in the page inventory (FR-001), that every survey item above is marked confirmed or rejected, and that every confirmed failure names a criterion, a component and a severity.

**Acceptance Scenarios**:

1. **Given** the page inventory, **When** the audit is done, **Then** each page has an automated result and a manual-checklist result.
2. **Given** a confirmed failure, **When** it is recorded, **Then** it has a WCAG criterion, the location (component or docs page), a severity (blocker / serious / moderate / minor), and a remediation task.

---

### User Story 2 - Night use without sight of the screen's detail (Priority: P1)

An observer using a screen reader, or keyboard only, can read the sky readings and the safety verdict, open Sun & Moon and the alerts, and change and save every setting, without a mouse and without being flooded with announcements.

**Why this priority**: These are the core jobs of the device. A blind or motor-impaired user who can't do them can't use the product at all.

**Independent Test**: With VoiceOver (macOS Safari / iOS) and NVDA (Windows, Firefox or Chrome), and separately keyboard only, complete the core task script (FR-008) on the device UI and the demo.

**Acceptance Scenarios**:

1. **Given** the dashboard is open with a screen reader, **When** readings update every second, **Then** nothing is announced unless the user asked for it, except a change in the safety verdict, which is announced once, politely.
2. **Given** keyboard only, **When** the user tabs through any page, **Then** every interactive element is reachable in a logical order, its focus is clearly visible, and nothing traps focus.
3. **Given** the Settings tabs, **When** the user presses the arrow keys on the tab list, **Then** focus moves between tabs, following the ARIA tabs pattern.
4. **Given** the alerts bell, **When** the user opens it, **Then** focus moves into the flyout. Escape closes it and focus returns to the bell.
5. **Given** an invalid setting, **When** the user saves, **Then** the error is announced and tied to its field, and focus goes to the first invalid field.

---

### User Story 3 - Low vision, zoom and phones (Priority: P2)

A user with low vision can read everything at 200% zoom, and on a 320 px-wide screen (the 400% reflow case) without scrolling sideways, and every text and control meets contrast in the dark theme.

**Why this priority**: The UI is dark and dense and is often used on a phone in the field. Contrast and reflow are the most likely failures.

**Independent Test**: Automated contrast checks pass; manual check at 200% browser zoom and at a 320 px viewport on every page.

**Acceptance Scenarios**:

1. **Given** any page at 320 px wide, **When** it is viewed, **Then** content reflows with no horizontal scroll. The only exceptions are data tables and charts, which scroll within their own box.
2. **Given** the Settings page on a phone, **When** the Demo panel button is showing, **Then** it doesn't cover the Save button or other controls.
3. **Given** any text, **When** it is measured, **Then** it has at least 4.5:1 contrast (3:1 for large text), and control edges and focus indicators have at least 3:1.

---

### User Story 4 - Stays accessible: checks in CI (Priority: P1)

A contributor who adds an inaccessible component finds out in their pull request, not from a user.

**Why this priority**: Without enforcement, any fix decays. This mirrors the coding-standards enforcement (spec 017).

**Independent Test**: Introduce a known violation (an icon button with no name, low-contrast text) on a branch; the accessibility CI check fails and names it. Remove it and the check passes.

**Acceptance Scenarios**:

1. **Given** a pull request, **When** CI runs, **Then** automated accessibility checks run on every page in the inventory, in each of its key states (FR-003), against the demo build.
2. **Given** existing violations recorded in a baseline, **When** a pull request adds none, **Then** CI passes. **When** it adds a new one, or one that the baseline doesn't list, **Then** CI fails.
3. **Given** a fixed violation, **When** CI runs, **Then** it reports that the baseline can shrink, and the baseline is never allowed to grow without an explicit, reviewed change.

---

### User Story 5 - Reduced motion and colour independence (Priority: P2)

A user who has asked their system for reduced motion sees no non-essential animation. A user with colour-vision deficiency can tell safe from unsafe, OK from failed, and clear from cloudy without relying on colour.

**Independent Test**: With `prefers-reduced-motion: reduce`, no animation or transition longer than a fade of 0.2 s plays. In greyscale mode, every status still reads correctly from text or shape.

**Acceptance Scenarios**:

1. **Given** reduced motion, **When** pages load, toasts appear or the Masonry layout changes, **Then** there is no movement animation.
2. **Given** greyscale, **When** the user looks at the safety card, the sensor statuses and the alerts, **Then** each state is clear from text or an icon.

---

### User Story 6 - Accessibility statement (Priority: P3)

Anyone can find out how accessible SQMeter is, what is known not to work, and how to report a problem.

**Independent Test**: The docs site has an Accessibility page, linked from the footer or navigation, with the target standard, the date of the last audit, the known exceptions and how to report a problem.

**Acceptance Scenarios**:

1. **Given** sqmeter.dev, **When** the user opens Accessibility, **Then** it states WCAG 2.2 AA as the target, the conformance status, known exceptions with reasons, and the issue-tracker route for reports.

---

### Edge Cases

- **Live data and screen readers**: values change every second. Only a verdict change (safe ⇄ unsafe) and new alerts are announced automatically. A user can always hear a value by moving to it. No region updates its accessible text more than about once every 2 s while focused.
- **Device offline / reconnecting**: the "connection lost" state is announced once, not on every retry.
- **Restart / OTA in progress**: progress is exposed as a progress bar with a value, and announced at most at 25% steps and on completion or failure.
- **Night use**: users want a dim screen. Contrast minimums still apply to the theme; screen brightness is the user's choice. If a red night-vision theme is ever added, it must meet the same contrast ratios, or the statement must document an exception for it with a reason.
- **Captive-portal WiFi setup page** (served in AP mode on a phone): it is in scope and must meet the same rules. Being small, it is a likely place for unlabelled inputs.
- **Demo-only UI** (Demo panel, tour from spec 018): in scope for the demo build; it must not be what breaks reflow or focus order.
- **Charts** with no data yet: the text alternative says so ("No data yet"), not an empty string.
- **Third-party docs theme (Material for MkDocs)**: upstream issues are recorded as exceptions, not worked around with heavy overrides.

## Requirements *(mandatory)*

### Functional Requirements

**Audit**

- **FR-001**: There MUST be a page inventory: every route of the device UI (Dashboard, Alpaca, System, Settings × each tab, Updates, WiFi setup, Not found), the captive-portal setup page, the demo-only controls, and every page of the docs site. The inventory MUST live in the repository, and CI MUST use it.
- **FR-002**: The audit MUST record, for each inventory entry, the automated result and the manual-checklist result (FR-008). Each failure MUST have a WCAG 2.2 criterion, a location, a severity and a remediation task. Every survey item A1–A15 MUST be confirmed or rejected with evidence.

**Automated checks**

- **FR-003**: Automated accessibility checks (an axe-class rules engine, run in a real browser) MUST run against every inventory page of the demo build in CI. Each page MUST be checked in its key states: the default state, a dialog or flyout open, a validation error showing, and an unsafe verdict. They MUST also run at a desktop and a 320 px viewport.
- **FR-004**: The docs site MUST be checked automatically in CI with the same engine on every built page.
- **FR-005**: Existing violations MUST be recorded in a baseline (rule × page), so the checks fail only on new violations. CI MUST also report baseline entries that no longer occur. The baseline MUST NOT grow except through an explicit, reviewed change. This follows the same pattern as the coding-standards baseline (spec 017).
- **FR-006**: The checks MUST be runnable locally with one command, with output that names the rule, the element and the page.

**Remediation (the rules the UI must meet)**

- **FR-007**: Every element of the device UI, docs and demo MUST meet WCAG 2.2 Level AA, except exceptions documented in the statement (FR-016). In particular:
  - text contrast is at least 4.5:1 (3:1 for large text);
  - control boundaries, chart lines that carry meaning, and focus indicators are at least 3:1;
  - every interactive element has a visible focus indicator that isn't hidden by sticky or floating content (the Demo panel, toasts);
  - every icon-only button and link has an accessible name;
  - targets are at least 24 × 24 CSS px, or have equivalent spacing;
  - the page has a skip link to main content, one `h1`, landmarks, and a correct `lang`;
  - forms have labels; errors are tied to their fields and announced;
  - nothing depends on hover alone (tooltips work on focus and tap and can be dismissed with Escape);
  - content reflows at 320 px.
- **FR-008**: A manual checklist MUST exist and be run for each release that changes the UI. It covers:
  - keyboard only;
  - VoiceOver on macOS and iOS, and NVDA on Windows;
  - 200% zoom;
  - 320 px reflow;
  - reduced motion;
  - greyscale / colour independence.

  For each, it runs the core task script: read the sky quality and the verdict; find why it is unsafe; open Sun & Moon; open and clear the alerts; change a setting and save; recover from a validation error; run an update check; join WiFi on the setup page.
- **FR-009**: Live updates MUST NOT cause continuous screen-reader announcements. A change in the safety verdict, and a new alert, MUST each be announced once through a polite live region. Connection loss and recovery MUST be announced once each. Other values MUST be readable on demand, not pushed.
- **FR-010**: Status MUST NOT be conveyed by colour alone. Safe/unsafe, sensor OK/failed/stale, cloud condition, and alert level each have text or an icon with a text equivalent. Any future alternative theme (for example a red night-vision theme) MUST meet FR-007's contrast, or appear as a documented exception with a reason.
- **FR-011**: Charts and sparklines MUST have a text alternative that gives their meaning:
  - the Night chart: dark from–to, moonrise/moonset, and the moon's illumination;
  - sparklines: the trend direction and range over the window shown.

  The alternative may be visible text or a programmatically linked description. Purely decorative graphics stay hidden from assistive technology.
- **FR-012**: Dialogs and flyouts (the alerts flyout, confirmation dialogs, the Demo panel, the spec 018 tour) MUST:
  - move focus into the dialog on open;
  - keep Tab within a modal dialog;
  - close on Escape;
  - return focus to the control that opened them.
- **FR-013**: Composite widgets MUST follow the matching ARIA Authoring Practices pattern: the Settings tabs (arrow keys, roving tabindex, a panel labelled by its tab), toggles (a switch role with state), the Masonry reorder (keyboard alternative already present, to be verified), and the progress meters (progressbar with value).
- **FR-014**: With `prefers-reduced-motion: reduce`, non-essential animation and transition MUST be removed or reduced to a short fade.

**Budget**

- **FR-015**: The accessibility fixes MUST NOT add a runtime dependency to the device UI. They MUST add no more than **4 KB gzipped** in total to the device UI's JavaScript and CSS, measured against the build before the work begins. The audit and CI tooling are development-only and MUST NOT ship in the device's filesystem image. The device filesystem is 512 KB, shared with the rest of the UI.

**Statement and standards**

- **FR-016**: The docs MUST include an Accessibility page, linked from every docs page's footer or navigation. It MUST give:
  - the target (WCAG 2.2 AA);
  - the current status per area (device UI, demo, docs);
  - the date of the last audit;
  - known exceptions with reasons and plans;
  - how to report a problem (GitHub issue with an "accessibility" label).
- **FR-017**: The project's coding standard MUST gain an accessibility section with numbered rules (A11Y-xx) that cover FR-007 and FR-009 to FR-014, so convergence can check them. Each rule MUST say whether CI enforces it automatically or the manual checklist covers it. If the coding standard from spec 017 has not merged yet, the rules MUST go in as an amendment to it.
- **FR-018**: New UI components MUST come with a check that covers their accessible name and role. This can be a component test query by role and name, or coverage in the automated page checks.

### Key Entities

- **Page inventory**: the list of pages and states to check. Each entry has a route, its states, viewports and an owner component.
- **Audit finding**: a WCAG criterion, a location, a severity, evidence (a screenshot or the rule output), and a status (open / fixed / exception).
- **Baseline**: the known violations per rule × page that CI tolerates while they are burned down.
- **Exception**: a documented, accepted non-conformance with a reason and a review date. Exceptions appear in the statement.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The automated checks report **zero** serious or critical violations on every inventory page, in every state and viewport, for the device UI, the demo and the docs. Remaining minor or moderate violations are listed as exceptions or tracked in the baseline with a burn-down.
- **SC-002**: Every step of the core task script (FR-008) can be done keyboard-only and with VoiceOver and NVDA, with no step needing sighted help.
- **SC-003**: Over 60 s on the dashboard with readings updating, a screen reader receives no automatic announcement unless the verdict changes or an alert arrives. When the verdict changes, it receives exactly one.
- **SC-004**: Every page reflows at 320 px with no horizontal page scroll. At 200% zoom, no content or function is lost.
- **SC-005**: All text and control contrast in the shipped theme meets FR-007: 100% of colour-token pairs in use pass.
- **SC-006**: A deliberately introduced violation fails CI in the pull request that introduces it (proven once for each check family: name, contrast, structure).
- **SC-007**: The device UI's gzipped JS and CSS grow by no more than 4 KB because of this work.
- **SC-008**: The Accessibility statement is live on sqmeter.dev and linked from every page.

## Assumptions

- **Target WCAG 2.2 Level AA.** It is the current W3C recommendation and the level most laws and procurement reference (EN 301 549 / the European Accessibility Act). AAA criteria are not targeted, except reduced motion (2.3.3), which is adopted because the UI animates on a dark screen used at night.
- **Tooling.** The automated engine is axe-core, run through Playwright, which the project already uses for the demo tests. The docs are checked by the same engine against the built mkdocs site. These are planning-level defaults; the plan may choose differently if it records why.
- **The demo build is the test target** for the device UI. It runs the same components as the device (spec 016), so a result for the demo holds for the device. Pages that exist only on the device (the captive portal) are checked from the device's build of the page.
- **Screen readers.** VoiceOver (macOS Safari, iOS Safari) and NVDA (Windows with Firefox or Chrome) are the reference pair. JAWS and TalkBack are best effort.
- **One dark theme.** There is one dark theme today, which also serves as the night theme. A red night-vision mode is not in scope here; FR-010 sets the rules for any future one. A light theme is not in scope.
- **Language.** The UI is English-only until the I18N work. Its `lang` is `en`. When translations arrive, `lang` must follow the active language; that spec carries the requirement.
- **No overlays.** No accessibility overlay or widget will be used. Fixes are made in the components.
- **Severity.** Severities follow axe's impact levels (critical, serious, moderate, minor) for automated findings. Manual findings use the same scale by judgement: a blocker is anything that stops a core task.
- **Budget.** The 4 KB gzipped budget is generous for ARIA attributes, a skip link, a focus helper, a live-region helper and CSS changes. It rules out shipping a dialog or focus-trap library; small in-house helpers are expected.
- **Dependencies.** The coding standard (spec 017) provides the baseline-and-burn-down approach and the place for the A11Y rules. The tour (spec 018) must meet FR-012 when it is built. The I18N work must preserve accessible names in every language.
