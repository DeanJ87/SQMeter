# Contract: A11Y rules (FR-017)

These rules extend the coding standard (spec 017). They are published as `docs/development/accessibility.md`.

**Enforcement** says how each rule is checked:
- **CI**: the axe Playwright checks, or a Vitest test, fail the build.
- **Manual**: the release checklist covers it.
- **Review**: a reviewer checks it in the PR.

| ID | Rule | Enforcement |
|---|---|---|
| A11Y-01 | Text meets 4.5:1 (3:1 large); control boundaries, meaningful chart lines and focus indicators meet 3:1. Use the theme tokens; `--dim` is the faintest text colour. | CI (axe `color-contrast`, token test) |
| A11Y-02 | Every interactive element shows a visible focus indicator (the global `:focus-visible` ring). Never `outline: none` without an equal replacement. Sticky or floating UI must not cover the focused element. | CI (axe partial) + Manual |
| A11Y-03 | Every icon-only button or link has an accessible name (`ariaLabel`). Decorative SVG is `aria-hidden`. | CI (axe `button-name`, `link-name`) |
| A11Y-04 | Form controls are labelled. Use `Field` (it labels its inputs and ties errors with `aria-describedby` / `aria-invalid`), or pass `ariaLabel`. | CI (axe `label`, `select-name`) |
| A11Y-05 | Targets are at least 24 × 24 CSS px, or have equivalent spacing. | Review + Manual |
| A11Y-06 | Pages keep the skip link, one `h1`, landmarks (`header`, `nav`, `main`) and `lang`. | CI (axe `region`, `page-has-heading-one`, `html-has-lang`) |
| A11Y-07 | No hover-only content. Tooltips use `InfoTip`: focusable, tappable, dismissable with Escape. | Review + Manual |
| A11Y-08 | Content reflows at 320 px with no horizontal page scroll. Tables and charts may scroll in their own box. | CI (axe at 320 px) + Manual |
| A11Y-09 | Live data never announces itself. Only a verdict change, a new alert, and connection loss and recovery are announced, once, through `announce()`. Never put `aria-live` on a value that updates. | Review + Manual |
| A11Y-10 | Status is never colour alone: each state has text or an icon with a text equivalent. | Review + Manual |
| A11Y-11 | Charts and sparklines have a text alternative stating their meaning (`role="img"` + label from `summariseSeries` or equivalent), and "No data yet" when empty. | CI (axe `svg-img-alt`) + Review |
| A11Y-12 | Dialogs and flyouts move focus in on open, close on Escape and return focus to their trigger (`useDialogFocus`). Modal dialogs also keep Tab inside. | Review + Manual |
| A11Y-13 | Composite widgets follow the ARIA APG: tabs (arrows, roving tabindex, labelled panel), switches, progress bars (`progressbar` with a value). | CI (component tests) + Review |
| A11Y-14 | Motion respects `prefers-reduced-motion`. New animation goes through CSS covered by the reduced-motion block, and JS scrolling uses `prefersReducedMotion()`. | Review + Manual |
| A11Y-15 | A new component comes with a test that finds it by role and accessible name, or its page is in the a11y inventory. | Review (FR-018) |
