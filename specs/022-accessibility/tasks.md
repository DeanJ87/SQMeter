# Tasks: Accessibility Audit and Remediation

**Input**: `specs/022-accessibility/` (plan.md, research.md, data-model.md, contracts/a11y-rules.md, quickstart.md)

**Tests**: These are requested by the spec (FR-003–FR-006, FR-018, SC-006), so test tasks are included.

## Phase 1: Setup

- [x] T001 Add `@axe-core/playwright` as a devDependency in `web/package.json` (dev only, not shipped, FR-015)
- [x] T002 Record the pre-work device UI size (JS 66.20 kB + CSS 6.81 kB gzipped) in `specs/022-accessibility/audit.md` (SC-007 baseline)

## Phase 2: Foundational (blocks every story)

- [x] T003 Create the page inventory: 12 entries, 5 state kinds, desktop and 320 px viewports, in `web/tests/a11y/inventory.ts` (FR-001)
- [x] T004 Write the axe page checks with the rule × page baseline, a "can shrink" report and `A11Y_UPDATE_BASELINE=1`, in `web/tests/a11y.spec.ts`, with `web/tests/a11y/baseline.json` (FR-003, FR-005, FR-006)
- [x] T005 [P] Write the shared helpers `announce()` (polite, 2 s dedupe), `useDialogFocus()`, `nextTabIndex()`, `prefersReducedMotion()` and `summariseSeries()` in `web/src/lib/a11y.ts`, with tests in `web/src/lib/__tests__/a11y.test.ts`
- [x] T006 [P] Add the token contrast test (text tokens ≥ 4.5:1, `--control-edge` ≥ 3:1 on every background token) in `web/src/__tests__/contrast.test.ts` (SC-005)

## Phase 3: User Story 1 - Audit (P1)

**Goal**: every page has a known result; A1–A15 are confirmed or rejected.

**Independent test**: audit.md covers every inventory entry and A1–A15, and each failure has a criterion, location, severity and task.

- [x] T007 [US1] Run the axe checks on the unmodified UI, record the "before" violations per page in `specs/022-accessibility/audit.md`, and seed `web/tests/a11y/baseline.json` from them (FR-002)
- [x] T008 [US1] Confirm or reject A1–A15 with evidence, and list the findings F-nn with criterion, location, severity and task, in `specs/022-accessibility/audit.md` (FR-002)

## Phase 4: User Story 2 - Screen reader and keyboard (P1)

**Goal**: the core task script works keyboard-only and with a screen reader, with no announcement flood.

**Independent test**: component tests by role and name pass, axe has no `label` / `select-name` / `button-name` violations, and the manual script passes.

- [x] T009 [US2] Label every Field input through context (`aria-labelledby` the label text, `aria-describedby` the error, `aria-invalid`) in `web/src/components/settings/controls.tsx` (A14, FR-007)
- [x] T010 [US2] Rebuild InfoTip as a button with `aria-describedby` to the tooltip, tap to toggle, Escape to dismiss and a 24 px target, in `web/src/components/ui.tsx` (A4)
- [x] T011 [US2] Make the Settings tabs follow the ARIA pattern (ids, `aria-controls`, roving tabindex, ←/→/Home/End, labelled tabpanel) in `web/src/components/Settings.tsx` (A5, FR-013)
- [x] T012 [US2] Move focus into the alerts flyout on open and back to the bell on Escape or close; label it by its heading, in `web/src/components/AlertsBell.tsx` (A6, FR-012)
- [x] T013 [US2] Give the Demo panel focus in and out, and Escape to close, in `web/src/demo/DemoPanel.tsx` (A13, FR-012)
- [x] T014 [US2] Add a polite live region and announcements: the verdict change (Dashboard), a new alert (AlertsBell), connection loss and recovery (Layout), in `web/src/components/Layout.tsx`, `Dashboard.tsx` and `AlertsBell.tsx` (A7, FR-009, SC-003)
- [x] T015 [US2] Add the skip link to `main#main`, `aria-current="page"` on nav, and check `lang="en"`, in `web/src/components/Layout.tsx` and `web/index.html` (A11)
- [x] T016 [US2] Add text alternatives for the sparklines (trend summary) and the Night chart (darkness, moon times, illumination; "No data yet"), in `web/src/components/Dashboard.tsx` and `web/src/components/NightChart.tsx` (A8, FR-011)
- [x] T017 [US2] Make ProgressMeter a `progressbar` with `aria-valuenow`, `-valuemin` and `-valuemax` in `web/src/components/ui.tsx` (FR-013)
- [x] T018 [P] [US2] Add component tests by role and name for InfoTip, Field labelling and errors, Settings tabs keyboard, the flyout focus and ProgressMeter, in `web/src/components/__tests__/a11y.test.tsx` (FR-018)

## Phase 5: User Story 3 - Low vision, zoom and phones (P2)

**Goal**: contrast and reflow pass everywhere, and nothing covers the focused control.

**Independent test**: axe has no `color-contrast` violations at either viewport; the token test passes; the manual 200% and 320 px checks pass.

- [x] T019 [US3] Change `--dim` to `#768798`, add `--control-edge: #55687a` for inputs, selects, toggles and checkboxes, and fix the contrast findings axe reports (input units, flyout heading and note), in `web/src/index.css` (A1, A2)
- [x] T020 [US3] Add a global `:focus-visible` ring, remove the bare `outline: none`, and set `scroll-padding-bottom` for the sticky save bar and toasts, in `web/src/index.css` (A3, FR-007)
- [x] T021 [US3] Give `.btn-sm`, `.note-action`, `.toast-close`, `.info-tip` and `.demo-panel-toggle` a 24 × 24 px minimum target in `web/src/index.css` (A12)
- [x] T022 [US3] Check 320 px reflow on every page (axe at 320 plus a no-horizontal-scroll assertion in `web/tests/a11y.spec.ts`) and fix any overflow (A13, SC-004)

## Phase 6: User Story 4 - CI keeps it fixed (P1)

**Goal**: a new violation fails the PR.

**Independent test**: a deliberate violation of each family fails locally, then passes once removed.

- [x] T023 [US4] Write the docs axe check with its own baseline, serving `site/`, in `web/tests/a11y-docs.spec.ts`, `web/playwright.docs.config.ts` and `web/tests/a11y/docs-baseline.json` (FR-004)
- [x] T024 [US4] Run both checks in CI: the demo check after "Build demo site", the docs check after "Build MkDocs site", in `.github/workflows/docs.yml` (FR-003, FR-004)
- [x] T025 [US4] Prove that each check family (name, contrast, structure) fails on a deliberate violation and passes without it; record the results in `specs/022-accessibility/audit.md` (SC-006)
- [x] T026 [US4] Shrink both baselines to what remains after the fixes; nothing serious or critical may remain unless listed as an exception (SC-001)

## Phase 7: User Story 5 - Reduced motion and colour independence (P2)

- [x] T027 [US5] Add a `prefers-reduced-motion` block that removes transforms and animations, and make the JS smooth scrolls respect it, in `web/src/index.css` and `web/src/components/Settings.tsx` (A10, FR-014)
- [x] T028 [US5] Check that every status dot and tone has text; add visually hidden text where it doesn't (`.sr-only` utility), in `web/src/components/Dashboard.tsx` and `web/src/index.css` (A9, FR-010)

## Phase 8: User Story 6 - Statement (P3)

- [x] T029 [US6] Write the accessibility statement (target, status per area, audit date, known exceptions, how to report), in `docs/accessibility.md`, linked in `mkdocs.yml` nav and the footer (`copyright`) (FR-016)
- [x] T030 [US6] Check the docs theme's contrast and fix it in `docs/stylesheets/sqmeter.css`; check screenshot alt text in `docs/**/*.md` (A15)

## Phase 9: Polish and cross-cutting

- [x] T031 Add the A11Y-01…15 rules and the manual checklist with the core task script to `docs/development/accessibility.md` (amends the coding standard; FR-008, FR-017) and add it to the `mkdocs.yml` nav
- [x] T032 Measure the device UI's gzipped size after the work and record it against the budget (≤ +4 KB) in `specs/022-accessibility/audit.md` (SC-007, FR-015)
- [x] T033 Run the full verification: `tsc`, `vitest`, `npm run build`, `npm run build:demo`, Playwright (demo, a11y, docs a11y) and `mkdocs build --strict`

## Dependencies

- Setup → Foundational (T003–T006) → US1 (T007–T008).
- US2, US3 and US5 then run in parallel, except that they all touch `index.css`; do those edits one after another.
- US4 T023–T024 can run any time after T004. T025–T026 come after the fixes.
- US6 can run any time.

## Implementation strategy

MVP = US1 + US2 + US4: measured, the core tasks usable, and enforced. Then US3, US5 and US6.
