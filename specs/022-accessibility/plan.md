# Implementation Plan: Accessibility Audit and Remediation

**Branch**: `feat/022-accessibility` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/022-accessibility/spec.md`

## Summary

Bring the device web UI, the demo and the docs site to WCAG 2.2 AA. The work happens in three parts:

1. **Measure.** axe-core runs through Playwright against every page and state in a committed page inventory, at desktop and 320 px. Existing violations live in a rule × page baseline that may only shrink. The same engine checks every page of the built docs site. The audit report records A1–A15 and the manual checklist.
2. **Fix it in the shared building blocks** (`ui.tsx`, `settings/controls.tsx`, `Layout.tsx`, `index.css`) rather than page by page:
   - colour tokens that meet contrast;
   - visible focus;
   - Field ↔ input labelling with errors tied to their field;
   - an accessible InfoTip;
   - ARIA tabs;
   - a dialog focus helper;
   - one polite live region with an `announce()` helper;
   - chart text alternatives;
   - reduced motion;
   - 24 px targets;
   - a skip link.
3. **Keep it fixed.** CI runs both checks. A11Y-xx rules extend the coding standard. Docs gain an accessibility statement and a manual checklist.

The device UI may grow by at most 4 KB gzipped. No runtime dependency is added.

## Technical Context

**Language/Version**: TypeScript 5 (strict), Preact 10.19, CSS; Python 3.12 (docs build); Markdown (mkdocs-material)

**Primary Dependencies**: The runtime has none new. Development adds `@axe-core/playwright` (axe-core 4.x) on top of the existing `@playwright/test`.

**Storage**: N/A. The baselines are JSON files under `web/tests/a11y/`.

**Testing**:
- Vitest and Testing Library for components, queried by role and name.
- Playwright and axe for pages: `web/tests/a11y.spec.ts` checks the demo and `web/tests/a11y-docs.spec.ts` checks the docs.
- `mkdocs build --strict`.

**Target Platform**: Browsers: evergreen Chrome, Firefox and Safari on desktop, and iOS Safari and Android Chrome. The UI is served from the ESP32 (LittleFS) and from demo.sqmeter.dev; the docs are served from sqmeter.dev.

**Project Type**: Embedded device with a web UI. Only the web UI and docs change; there are no firmware changes.

**Performance Goals**: Announcements: at most one per verdict change, per new alert and per connection loss or recovery. There are no per-reading announcements.

**Constraints**:
- At most +4 KB gzipped of JS and CSS (FR-015), measured against the pre-work build: JS 66.20 kB plus CSS 6.81 kB, 73.01 kB in total.
- No overlay widgets and no focus-trap or dialog libraries.

**Scale/Scope**:
- 12 inventory entries (8 routes, 6 of them Settings tabs), 5 state kinds and 2 viewports.
- About 40 docs pages.

## Constitution Check

*GATE: checked before Phase 0 and re-checked after design.*

| Principle | Touched? | How it is satisfied |
|---|---|---|
| I. Fail-safe verdict | Display only | The verdict is unchanged. A change in it is now also announced (FR-009). |
| II. Alpaca conformance | No | — |
| III. Testable pure logic | Yes | New helpers (`announce`, `useDialogFocus`, tab keyboard logic, sparkline summary, contrast pairs) have Vitest tests. Pages are covered by the axe Playwright checks against the MSW/WASM demo. |
| IV. Resource budgets | Yes | The +4 KB gzipped budget is measured and reported in the PR. No firmware change. |
| V. Quiet, consistent UI | Yes | All fixes go into the shared building blocks; no new button or card styles. Visual changes are limited to brighter dim text, visible focus rings and stronger control edges. |
| VI. Security | No | — |
| VII. Docs move with behaviour | Yes | Adds an accessibility statement and a manual checklist. The A11Y rules are in the development docs, and `mkdocs --strict` passes. |

Workflow: one PR from `main`, no AI attribution. **Result: PASS**, with no violations to justify.

## Project Structure

### Documentation (this feature)

```text
specs/022-accessibility/
├── plan.md
├── research.md        # decisions R1–R16
├── data-model.md      # inventory, finding, baseline, exception
├── quickstart.md      # how to run and verify
├── audit.md           # FR-002: A1–A15 confirmed/rejected, findings, manual results
├── contracts/
│   └── a11y-rules.md  # A11Y-01..A11Y-14: the rule set (FR-017)
└── tasks.md
```

### Source Code (repository root)

```text
web/
├── src/
│   ├── index.css                 # tokens (--dim, --control-edge), :focus-visible, targets, reduced motion, .sr-only, skip link
│   ├── lib/a11y.ts               # announce(), useDialogFocus(), prefersReducedMotion(), summariseSeries()
│   ├── components/
│   │   ├── ui.tsx                # InfoTip (button + aria-describedby + Escape), ProgressMeter (progressbar), Sparkline label
│   │   ├── settings/controls.tsx # Field context → aria-labelledby / aria-describedby / aria-invalid on inputs
│   │   ├── Layout.tsx            # skip link, main#main, aria-current, live region
│   │   ├── Settings.tsx          # ARIA tabs: ids, roving tabindex, arrows/Home/End, tabpanel labelled
│   │   ├── AlertsBell.tsx        # dialog focus in/out, announce new alerts
│   │   ├── Dashboard.tsx         # sparkline text alternative, verdict + connection announcements
│   │   ├── NightChart.tsx        # text alternative (dark from–to, moon)
│   │   └── __tests__/…           # role/name tests for the above
│   └── demo/DemoPanel.tsx        # dialog focus in/out (FR-012)
├── tests/
│   ├── a11y/inventory.ts         # FR-001 page inventory
│   ├── a11y/baseline.json        # FR-005 rule × page baseline
│   ├── a11y/docs-baseline.json   # FR-005 for the docs
│   ├── a11y.spec.ts              # FR-003/FR-006
│   └── a11y-docs.spec.ts         # FR-004
├── playwright.docs.config.ts     # serves ../site for the docs check
docs/
├── accessibility.md              # FR-016 statement
├── development/accessibility.md  # FR-008 checklist + FR-017 rules (amends the coding standard)
└── stylesheets/sqmeter.css       # docs contrast fixes
.github/workflows/docs.yml        # runs both checks in CI
```

**Structure Decision**: Web UI plus docs only. The new shared helpers live in `web/src/lib/a11y.ts`, next to the other UI logic in `lib/`. The tests and baselines sit with the existing Playwright tests.

## Complexity Tracking

None.
