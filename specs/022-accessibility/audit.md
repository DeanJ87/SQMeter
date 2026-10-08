# Accessibility audit (FR-002)

- **Date**: 2026-10-08
- **Target**: WCAG 2.2 AA
- **Engine**: axe-core 4.x through Playwright (Chromium), tags `wcag2a`, `wcag2aa`, `wcag21a`, `wcag21aa`, `wcag22aa`. The page checks add two of their own: no sideways scroll (reflow) and exactly one `h1`.
- **Inventory**: `web/tests/a11y/inventory.ts`.
  - 12 entries: the 8 routes, one of which is Settings checked tab by tab (6 tabs).
  - States: default; alerts flyout open; Demo panel open; validation error; unsafe (Rain).
  - Viewports: 1280 × 800 and 320 × 640.
- **Docs**: all 31 built pages (33 after this work adds 2).

## Automated results

### Device UI and demo: before

| Page | Violations (impact) |
|---|---|
| dashboard | `color-contrast` (serious): alerts flyout heading and "No alerts" note (`--dim`) |
| settings-device | `label` (critical): every input in a `Field` |
| settings-network | `label`, `select-name` (critical) |
| settings-time | `label`, `select-name` (critical); `color-contrast` (serious): `.input-unit` |
| settings-sensors | `label`, `select-name` (critical); `color-contrast` (serious) |
| settings-safety | `label` (critical) |
| settings-alerts | `label` (critical); `color-contrast` (serious): `.input-unit` |
| alpaca, system, updates, wifi, not-found | none |

That made 7 of 12 pages fail, with 3 distinct rules. The baseline was seeded with them in commit `1f60d41`.

### Device UI and demo: after

All 12 pages pass, in every state at both viewports: no axe violations, no sideways scroll, and one `h1`. **The baseline is empty** (`{}`).

### Docs: before

All 31 pages had `color-contrast` (serious) on `.md-copyright` and the table-of-contents title (`--sqm-dim`). 22 pages also had `link-in-text-block` (serious): links in text were distinguished by colour only, at 1.64:1 against the text.

### Docs: after

All 33 pages pass. The docs baseline is empty.

## Survey items A1–A15

| # | Finding | Status | Evidence / fix |
|---|---|---|---|
| A1 | `--dim` text 3.13:1 on the panel and 2.94:1 on panel-2 | **Confirmed, fixed** | axe `color-contrast` on the flyout and units. `--dim` is now `#768798` (≥ 4.70:1); `contrast.test.ts` guards every token pair (SC-005). |
| A2 | `--line` control edges 1.26:1 | **Confirmed, fixed** | New `--control-edge #55687a` (≥ 3.01:1) on inputs, input groups, toggles and the InfoTip. Decorative card borders keep `--line`. |
| A3 | `outline: none` on `.input` and `.info-tip` | **Confirmed, fixed** | A global `:focus-visible` 2 px cyan ring (≈ 9:1). Input groups show the ring on the group. `scroll-padding` keeps the focused element clear of the save bar and toasts. |
| A4 | InfoTip hid its text and had no Escape or tap support | **Confirmed, fixed** | It's now a `<button>` with `aria-describedby`, toggles on tap, is dismissed by Escape and has a 24 px target. Tests: `a11y.test.tsx`. |
| A5 | Tabs had no arrow keys or roving tabindex; the panel wasn't labelled | **Confirmed, fixed** | APG tabs: ←/→/Home/End, roving tabindex, `aria-controls`, panel `aria-labelledby`. The error dot is a description. Test: `a11y.test.tsx`. |
| A6 | Flyout focus wasn't managed | **Confirmed, fixed** | `useDialogFocus`: the heading takes focus on open, and Escape returns focus to the bell. Non-modal (R10). Test: `a11y.test.tsx`. |
| A7 | Nothing was announced, including the verdict | **Confirmed, fixed** | One polite live region. Announced: the verdict change (with reasons), a new alert, and connection lost and restored once each. Update progress is announced every 25% and on completion or failure. 2 s dedupe. Test: `lib/__tests__/a11y.test.ts`. |
| A8 | Sparkline hidden; Night chart's label was generic | **Confirmed, fixed** | The sparkline is `role="img"` with its trend ("no data yet" when empty). The Night chart's label gives darkness from–to, moon up times and illumination. |
| A9 | Status dots, colour only | **Rejected** | Every `StatusDot` sits next to its text ("Live", "Stale", "Connecting…"). Pills carry text. The Settings tab error dot gained a text description. |
| A10 | No reduced motion | **Confirmed, fixed** | A `prefers-reduced-motion` block cuts animations and transitions; JS scrolls use `scrollBehavior()`. |
| A11 | No skip link | **Confirmed, fixed** | "Skip to main content" moves focus to `main#main`. Nav buttons have `aria-current="page"`. |
| A12 | Small targets | **Partly confirmed, fixed** | The InfoTip (16 px) now has a 24 px hit area. `.toast-close`, `.note-action` and the Demo button have a 24 px minimum. `.btn-sm` is already 27 px. |
| A13 | Demo panel covering Save; focus | **Confirmed, fixed** | The overlap was fixed in #86 and is guarded by `demo.spec.ts`. Focus in and out and Escape were added (FR-012). |
| A14 | Errors not tied to fields | **Confirmed, fixed** | The `Field` context sets `aria-labelledby`, `aria-describedby` and `aria-invalid` on its inputs. On a failed save, the toast is announced (`role="status"`) and focus moves to the first invalid field. |
| A15 | Docs contrast, alt text, tables | **Confirmed (contrast, links), fixed** | `--sqm-dim` matches the UI; links in body text are underlined. Screenshot alt text is present on all 13 images. Tables are plain Markdown tables with header rows. |

## Other findings

| ID | Criterion | Location | Severity | Status |
|---|---|---|---|---|
| F-01 | 1.3.1 / 2.4.6 | Two `h1`s on the WiFi setup and Not-found pages | moderate | Fixed: the page titles are now `h2.page-title` / `.not-found-title`; checked by `single-h1` |
| F-02 | 4.1.2 | Progress meters had no role or value | moderate | Fixed: `role="progressbar"` with a value and label |
| F-03 | 4.1.2 | Toggles read as checkboxes | minor | Fixed: `role="switch"` |

## CI catches regressions (SC-006)

Each family was proven on a scratch change, then reverted:

| Family | Deliberate violation | Result |
|---|---|---|
| Name | `<button><svg aria-hidden/></button>` on Not-found | `button-name (critical) on not-found` — failed ✔ |
| Contrast | `--muted: #333333` | `color-contrast (serious) on system` — failed ✔, and `contrast.test.ts` failed 4 tests ✔ |
| Structure | an unlabelled `<select>` on Not-found | `select-name (critical) on not-found` — failed ✔ |

## Budget (SC-007, FR-015)

| | JS gzip | CSS gzip | Total |
|---|---|---|---|
| Before | 66.20 kB | 6.81 kB | 73.01 kB |
| After | 67.93 kB | 7.13 kB | 75.06 kB |
| Change | +1.73 kB | +0.32 kB | **+2.05 kB** (budget +4 KB) ✔ |

No runtime dependency was added; `@axe-core/playwright` is dev-only.

## Manual checklist (FR-008, SC-002)

**Not yet run.** It needs a person with VoiceOver, NVDA and real devices. The checklist and core task script are in `docs/development/accessibility.md`. Until they're run, the statement lists the status as "partially conformant".

| Check | Result |
|---|---|
| Keyboard only | pending |
| VoiceOver macOS / iOS | pending |
| NVDA | pending |
| 200% zoom | pending |
| 320 px reflow | automated ✔ (no sideways scroll on any page or state); manual pending |
| Reduced motion | pending |
| Greyscale | pending |
