# Accessibility (development)

How the web UI, the demo and these docs stay at **WCAG 2.2 AA** ([spec 022](https://github.com/DeanJ87/SQMeter/tree/main/specs/022-accessibility)). The public statement is [Accessibility](../accessibility.md).

!!! note "Part of the coding standard"
    These A11Y rules amend the project coding standard (spec 017). Until that standard's document lands, this page is where they live; afterwards they become its *Accessibility* section, with the same rule IDs.

## Checks

```bash
cd web
npm run build:demo
npx playwright test tests/a11y.spec.ts          # every page × state × desktop and 320 px

# docs (from the repo root: mkdocs build --strict)
npx playwright test -c playwright.docs.config.ts
```

- **Device UI pages and states:** listed in `web/tests/a11y/inventory.ts`. A new route or dialog goes there.
- **Known violations:** listed per page in `web/tests/a11y/baseline.json` and `docs-baseline.json`. Both are empty today and may only shrink.
  - A new violation fails the check, which names the rule, the element and the page.
  - Refresh a baseline with `A11Y_UPDATE_BASELINE=1` only to shrink it, or with a reason in the PR.
- **CI:** both checks run in the *Deploy Docs & Demo* workflow on every pull request that touches `web/` or `docs/`.
- **Colour tokens:** `src/__tests__/contrast.test.ts` checks every text token against every background (4.5:1), and `--control-edge` against them (3:1).

## Rules

**Enforced by** says how each rule is checked: **CI** means the build fails, **Review** means a reviewer checks it in the PR, and **Manual** means the release checklist below.

| ID | Rule | Enforced by |
|---|---|---|
| A11Y-01 | Text meets 4.5:1 (3:1 large); control edges, meaningful chart lines and focus indicators meet 3:1. Use the theme tokens; `--dim` is the faintest text colour and `--control-edge` the edge of inputs. | CI |
| A11Y-02 | Every interactive element shows the global `:focus-visible` ring. Never `outline: none` without an equal replacement. Sticky or floating UI must not cover the focused element. | CI (partly) + Manual |
| A11Y-03 | Icon-only buttons and links have an accessible name (`ariaLabel`). Decorative SVG is `aria-hidden`. | CI |
| A11Y-04 | Form controls are labelled. Put inputs in a `Field`, which labels them and ties its error to them (`aria-describedby`, `aria-invalid`), or pass `ariaLabel`. | CI |
| A11Y-05 | Targets are at least 24 × 24 CSS px, or have equivalent spacing. | Review + Manual |
| A11Y-06 | Keep the skip link, one `h1`, the landmarks (`header`, `nav`, `main`) and `lang`. | CI |
| A11Y-07 | Nothing appears only on hover. Use `InfoTip`: focusable, tappable, closes with Escape. | Review + Manual |
| A11Y-08 | Content reflows at 320 px with no sideways page scroll; tables and charts scroll in their own box. | CI + Manual |
| A11Y-09 | Live data never announces itself. Only a verdict change, a new alert, and connection loss and recovery are announced, once, through `announce()` / `useAnnounceChange()`. Never put `aria-live` on an updating value. | Review + Manual |
| A11Y-10 | Status is never colour alone: each state has text, or an icon with a text equivalent. | Review + Manual |
| A11Y-11 | Charts and sparklines have a text alternative giving their meaning (`role="img"` with a label from `summariseSeries` or equivalent), and "no data yet" when empty. | CI + Review |
| A11Y-12 | Dialogs and flyouts move focus in when opened, close on Escape and return focus to their trigger (`useDialogFocus`). Modal dialogs also keep Tab inside. | Review + Manual |
| A11Y-13 | Composite widgets follow the WAI-ARIA patterns: tabs (arrows, roving tabindex, labelled panel), switches (`role="switch"`), progress bars (`role="progressbar"` with a value). | CI (component tests) + Review |
| A11Y-14 | Motion respects `prefers-reduced-motion`. New animation goes in CSS, which the reduced-motion block covers; JS scrolling uses `scrollBehavior()`. | Review + Manual |
| A11Y-15 | A new component comes with a test that finds it by role and accessible name, or its page is in the inventory. | Review |

The helpers are in `web/src/lib/a11y.ts`.

## Manual checklist

Run this before a release that changes the UI, and record the result in the release PR.

| Check | How |
|---|---|
| Keyboard only | Unplug the mouse. Tab through every page: the order is logical, focus is always visible, and nothing traps focus. Esc closes the alerts flyout and the Demo panel, and focus returns to their button. |
| VoiceOver (macOS Safari, iOS Safari) | Run the task script. Fields read their label (and any error); "?" tips read their text; tabs say "tab, 2 of 6". |
| NVDA (Windows, Firefox or Chrome) | Run the task script. |
| 200% zoom | Every page at 200% browser zoom: nothing cut off, every function still there. |
| 320 px | Every page in a 320 px-wide window: no sideways scroll; the Demo button and toasts don't cover Save. |
| Reduced motion | Turn on "reduce motion" in the OS: no movement when pages load, toasts appear or cards are rearranged. |
| Greyscale | Turn on greyscale in the OS: safe vs unsafe, sensor OK vs failed, and alert levels are all clear from their text. |

### Core task script

1. Read the sky quality and the safety verdict on the dashboard.
2. Find out why it's unsafe: open the Rain scenario in the demo, or cover the rain sensor.
3. Open Sun & Moon and hear tonight's darkness and moon times.
4. Open the alerts, then clear them.
5. Change a setting and save.
6. Enter an invalid value, save, and recover from the error.
7. Run an update check on Updates.
8. Join WiFi on the setup page (`/wifi`).

Sixty seconds on the dashboard with a screen reader running should produce no announcements unless the verdict changes or an alert arrives.
