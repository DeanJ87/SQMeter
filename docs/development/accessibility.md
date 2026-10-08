# Accessibility (development)

How the web UI, the demo and these docs stay at **WCAG 2.2 AA** ([spec 022](https://github.com/DeanJ87/SQMeter/tree/main/specs/022-accessibility)). The public statement is [Accessibility](../accessibility.md).

The rules every change must meet are the **A11Y** section of the [coding standard](coding-standards.md#accessibility-a11y). This page covers how they're checked, and the manual checklist.

## Checks

```bash
cd web
npm run build:demo
npx playwright test tests/a11y.spec.ts tests/a11y-announce.spec.ts   # every page × state × desktop and 320 px; live-region quiet

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

See [Coding Standards → Accessibility (A11Y)](coding-standards.md#accessibility-a11y): A11Y-01 to A11Y-15, each marked with how it's checked. The helpers they refer to are in `web/src/lib/a11y.ts`.

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
