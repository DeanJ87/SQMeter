# Research: Accessibility Audit and Remediation

The decisions below were made without asking questions, in line with the build brief. Each says what was chosen and why.

## R1 Automated engine: axe-core through Playwright

- **Decision**: `@axe-core/playwright` (dev only), tags `wcag2a wcag2aa wcag21a wcag21aa wcag22aa`, run in Chromium against the demo build (`vite preview`).
- **Rationale**:
  - The spec's assumption.
  - Playwright already runs the demo tests and the docs screenshots in CI.
  - axe has no false-positive policy, so a failure is worth acting on.
- **Alternatives**:
  - Lighthouse: coarser, one page per run.
  - pa11y: a second browser stack.
  - Both rejected.

## R2 Baseline: rule × page JSON, update by env var

- **Decision**:
  - `web/tests/a11y/baseline.json` maps an inventory id to a list of rule ids.
  - A finding fails when its rule isn't listed for that page.
  - Listed rules that no longer occur are printed as "baseline can shrink" and attached as a test annotation.
  - `A11Y_UPDATE_BASELINE=1` rewrites the file. It's a reviewed diff in the PR, so growth is visible (FR-005).
- **Rationale**:
  - Rule × page is the granularity the spec names.
  - Element-level keys churn with every markup change.
- **Alternative**: a count per rule/page. Rejected because counts drift with page content (the number of sensors, alerts).

## R3 Inventory and states

- **Decision**: the inventory is `web/tests/a11y/inventory.ts`. It has 12 entries (all routes plus each Settings tab) and these states:
  - `default`;
  - `dialog`: the alerts flyout;
  - `demo-panel`;
  - `error`: an empty device name, then Save;
  - `unsafe`: `?scenario=rain`.

  Each state runs at 1280 × 800 and at 320 × 640, in a fresh browser context so the demo's sessionStorage doesn't leak between states.
- **Rationale**: these are the key states in FR-003. The captive-portal setup page is the SPA's `/wifi` route (WebServer redirects probes to `/wifi`), so the demo covers it.

## R4 Docs check

- **Decision**:
  - `web/tests/a11y-docs.spec.ts` with `web/playwright.docs.config.ts`, which serves `../site` with `python3 -m http.server`.
  - Pages are every `index.html` under `site/`, excluding `site/demo/` (a redirect) and search.
  - A separate baseline, `docs-baseline.json`.
- **Rationale**: the same engine and pattern (FR-004), and Python is already in the docs job.

## R5 Colour tokens

- **Decision**:
  - `--dim` goes from `#566675` (3.13:1 on the panel, 2.94:1 on panel-2) to `#768798` (5.01:1 / 4.70:1).
  - New `--control-edge: #55687a` (3.21:1 on the panel, 3.01:1 on panel-2) for the boundaries of inputs, selects, toggles and checkboxes.
  - `--line` stays for decorative card and section dividers. Those aren't UI component boundaries under 1.4.11.
  - A Vitest test parses `:root` in `index.css` and asserts every text token ≥ 4.5:1 and `--control-edge` ≥ 3:1 against every background token (SC-005).
- **Rationale**:
  - It keeps the quiet look (Principle V) while meeting 1.4.3 and 1.4.11.
  - Raising every border to 3:1 would make the dense dashboard busy for no conformance gain.

## R6 Focus indicator

- **Decision**:
  - A global `:focus-visible { outline: 2px solid var(--cyan); outline-offset: 2px }`.
  - Remove `outline: none` from `.input:focus` and `.info-tip:focus`; the border recolour stays as extra feedback.
  - `scroll-padding-bottom` on `html` so focused controls aren't hidden by the sticky save bar or toasts (2.4.11).
- **Rationale**: cyan on the backgrounds is about 9.5:1, so the focus ring meets 2.4.7 and 2.4.11 in every theme state.

## R7 Field labelling and errors

- **Decision**:
  - `Field` gets an id with `useId()` (Preact 10.19 has it) and provides a context `{ labelId, errorId, invalid }`.
  - `NumberInput`, `TextInput` and `SelectInput` read it and set `aria-labelledby`, plus `aria-describedby` / `aria-invalid` when there's an error. An explicit `ariaLabel` wins.
  - The error `Note` gets that id.
  - The label text sits in its own span, so the InfoTip button's name isn't part of the input's name.
- **Rationale**:
  - One change fixes every `label` / `select-name` violation axe found on 6 Settings tabs (3.3.1, 1.3.1, 4.1.2).
  - No call sites change, which keeps the merge clean with builders 020 and 021.
- **Alternative**: thread `id`/`for` through every call site. Rejected: there are about 80 call sites, many on files other builders are editing.

## R8 InfoTip

- **Decision**:
  - A real `<button type="button">` named "More information".
  - `aria-describedby` points at the tooltip, which has `role="tooltip"` and an id, so its text is read.
  - Shown on hover and focus. Clicking or tapping toggles it open (`aria-expanded`). Escape hides it until the next hover or focus (1.4.13).
  - The target is 24 × 24 (2.5.8).
- **Rationale**: it fixes A4 entirely, in one component.

## R9 Settings tabs

- **Decision**: the ARIA tabs pattern, automatic activation:
  - each tab has an id (`settings-tab-<id>`) and `aria-controls`;
  - roving `tabIndex` (0 on the selected tab, −1 on the others);
  - ←/→/Home/End move focus and select;
  - the panel has `id` and `aria-labelledby`, and `tabIndex=0`.

  The key handling is a pure function, `nextTabIndex(key, index, count)`, in `lib/a11y.ts`, unit tested.
- **Rationale**: it's the WAI-ARIA APG pattern (A5, FR-013).

## R10 Dialogs and flyouts

- **Decision**:
  - The alerts flyout and the Demo panel are **non-modal** dialogs. They don't block the page, so FR-012's Tab-containment clause (modal dialogs only) doesn't apply.
  - `useDialogFocus(open, containerRef, triggerRef)` moves focus into the container on open (its heading has `tabIndex=-1`, otherwise the first focusable element) and returns it to the trigger when the dialog closes, if focus was inside it.
  - Escape closes both. The Demo panel gains an Escape handler.
- **Rationale**: it fixes A6 and FR-012 with about 30 lines, without a focus-trap library (FR-015). A future modal (the spec 018 tour) can add a trap using the same helper.

## R11 Live announcements

- **Decision**:
  - One visually hidden `role="status" aria-live="polite"` region in `Layout`. `announce(text)` writes to it, clearing it first so a repeat is re-read.
  - It dedupes the same text within 2 s.
  - Announcements:
    - **Safety verdict change** (Dashboard, from `/ws/sensors` `safety.safe`): only on a change after the first value. "Observatory unsafe: <reasons>" / "Observatory safe".
    - **New alert** (AlertsBell): when the newest id increases after the first load, "New alert: <title>".
    - **Connection** (Layout, via `/ws/status` `connected`): "Connection to the device lost" once, and "Reconnected" once.
  - The toasts' existing `role="status"` stays. Save results and validation summaries are already announced through it.
- **Rationale**: FR-009 and SC-003; no per-reading chatter.

## R12 Charts

- **Decision**:
  - Sparklines become `role="img"` with `aria-label` from `summariseSeries(values, label, unit)`, e.g. "SQM trend: rising, 20.9 to 21.4 over the last 15 readings", or "no data yet".
  - NightChart's label is built from what it already computes: dark from–to (or "no full darkness tonight"), moonrise and moonset, and illumination.
- **Rationale**: FR-011, 1.1.1; nothing visible changes.

## R13 Status by colour alone

- **Decision**:
  - Audit every `status-dot` / tone use (A9).
  - Where a dot has no adjacent text, add visually hidden text ("OK" / "Fault").
  - Safe/unsafe and alert levels already have text labels; verify in audit.md.
- **Rationale**: 1.4.1.

## R14 Reduced motion

- **Decision**:
  - `@media (prefers-reduced-motion: reduce)` sets every animation and transition to at most 0.01 ms, except opacity fades.
  - JS smooth scrolling (`scrollIntoView({behavior:'smooth'})`) uses `'auto'` when reduced (`prefersReducedMotion()`).
  - Masonry drag has no animation of its own.
- **Rationale**: FR-014 / 2.3.3.

## R15 Targets, skip link, landmarks

- **Decision**:
  - `.btn-sm`, `.note-action`, `.toast-close`, `.info-tip` and the Demo toggle get a `min-height` and `min-width` of 24 px.
  - A skip link "Skip to main content" → `main#main` (`tabIndex=-1`).
  - Nav buttons get `aria-current="page"` when active.
  - `lang="en"` is confirmed in `index.html`.
- **Rationale**: 2.5.8, 2.4.1, 2.4.8 support.

## R16 Rules, statement, checklist

- **Decision**:
  - The A11Y-01…A11Y-14 rules go in `docs/development/accessibility.md`, with each rule marked "CI" or "manual" (FR-017). That page also holds the manual checklist and the core task script (FR-008).
  - The spec 017 coding standard isn't on `main` yet, so the page states that it amends the coding standard. Spec 017's build moves the rules into `coding-standards.md` under an "Accessibility" section.
  - `docs/accessibility.md` is the statement (FR-016). It's linked in the nav (Reference) and from every page's footer through Material's `copyright` slot.
- **Rationale**: FR-017's "amendment" path, without depending on 017's files.

## R17 Budget measurement

- **Decision**: compare the gzipped sizes Vite reports for `npm run build` before (JS 66.20 kB, CSS 6.81 kB) and after, and record them in the PR and audit.md (SC-007).
- **Rationale**: these are the numbers the device's LittleFS image contains.
