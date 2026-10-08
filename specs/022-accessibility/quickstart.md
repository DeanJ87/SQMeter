# Quickstart: verifying accessibility

## Automated: device UI (demo build)

```bash
cd web
npm run build:demo
npx playwright test tests/a11y.spec.ts
```

**Expected**:
- Every inventory page passes.
- Notes saying `[a11y] <page>: no longer violates <rule>` mean the baseline can shrink. Shrink it with:

  ```bash
  A11Y_UPDATE_BASELINE=1 npx playwright test tests/a11y.spec.ts
  ```

  Then review the diff in `tests/a11y/baseline.json`.

## Automated: docs

```bash
mkdocs build --strict          # repo root
cd web && npx playwright test -c playwright.docs.config.ts
```

## Prove that CI catches a regression (SC-006)

Do this on a scratch branch for each family, then revert:
- **Name:** add `<button><svg aria-hidden="true"/></button>` to `Dashboard.tsx`. Expect `button-name` on `dashboard`.
- **Contrast:** set `--muted: #333`. Expect `color-contrast`, plus a failing `contrast.test.ts`.
- **Structure:** remove `aria-label` from a `SelectInput` outside a `Field`. Expect `select-name`.

## Unit tests

```bash
cd web && npx vitest run
```

These cover:
- Tab keyboard logic.
- `announce()` dedupe.
- `summariseSeries`.
- The InfoTip role and description.
- Field labelling and errors.
- Token contrast pairs.

## Bundle budget (SC-007)

```bash
cd web && npm run build
```

Compare the reported gzip sizes with the baseline: JS 66.20 kB plus CSS 6.81 kB. The total may grow by at most 4 KB.

## Manual (FR-008)

Follow `docs/development/accessibility.md` → *Manual checklist* with VoiceOver, NVDA, keyboard only, 200% zoom, 320 px, reduced motion and greyscale. Record the results in `specs/022-accessibility/audit.md`.
