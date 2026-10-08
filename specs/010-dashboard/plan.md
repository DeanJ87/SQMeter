# Implementation Plan: Dashboard, Demo and Screenshots

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/010-dashboard/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`Dashboard.tsx` builds the card list from `/ws/sensors` and `/ws/status`; `Masonry.tsx` positions
cards and handles arranging; order is in localStorage. The demo build runs the UI against MSW mocks
(`web/src/mocks`); the docs workflow builds the demo and runs `web/tests/screenshots.spec.ts` to
write PNGs to `docs/assets/screenshots/`.

## Technical Context

**Language/Version**: TypeScript (Preact, Vite)
**Primary Dependencies**: MSW, Playwright (screenshots)
**Storage**: localStorage (card order)
**Testing**: Vitest (Masonry, SunMoonCard), Playwright screenshots
**Target Platform**: Browser; GitHub Pages (demo + docs)
**Project Type**: Web UI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| V. Quiet UI | Hidden cards for disabled hardware; phone width | Yes |
| VII. Docs | Dashboard documented with current screenshots | No dashboard guide; screenshots generated but unused |

## Project Structure

### Documentation (this feature)

```text
specs/010-dashboard/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
web/src/components/Dashboard.tsx, Masonry.tsx, SafetyCard.tsx, SunMoonCard.tsx, AlertsBell.tsx, Layout.tsx
web/src/mocks/handlers.ts, web/src/mocks/data.ts
web/tests/screenshots.spec.ts, web/playwright.config.ts
.github/workflows/docs.yml
docs/assets/screenshots/, docs/index.md, docs/live-demo.md, mkdocs.yml
```

**Structure Decision**: Web UI with MSW-backed demo and Playwright screenshots.

## Complexity Tracking

None.
