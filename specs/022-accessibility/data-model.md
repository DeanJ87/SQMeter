# Data Model: Accessibility Audit and Remediation

## Inventory entry (`web/tests/a11y/inventory.ts`)

| Field | Type | Rules |
|---|---|---|
| `id` | string | Unique and stable; used as the baseline key |
| `route` | string | A hash route in the demo (`#/settings?tab=alerts`) |
| `component` | string | The owning component, named in failure messages |
| `states` | `('default' \| 'dialog' \| 'demo-panel' \| 'error' \| 'unsafe')[]` | Must include `default` |

Every entry is checked at each `VIEWPORTS` size: desktop 1280 × 800 and phone 320 × 640.

## Docs page (derived)

Every `site/**/index.html` except `site/demo/` (a redirect) and `site/search/`. Its id is the path relative to `site/`, for example `user-guide/alerts/`.

## Baseline (`web/tests/a11y/baseline.json`, `docs-baseline.json`)

`{ [pageId: string]: string[] /* axe rule ids, sorted */ }`

- A page with no tolerated violations has no key.
- The baseline may only shrink. Any growth must be a reviewed diff with a reason in the PR, which a reviewer sees as JSON lines being added.

## Finding (`specs/022-accessibility/audit.md`)

| Field | Values |
|---|---|
| id | `F-nn`, or `A1`–`A15` for survey items |
| criterion | WCAG 2.2 SC number |
| location | component or docs page |
| severity | blocker / serious / moderate / minor (axe impact for automated findings) |
| evidence | axe rule output or a manual observation |
| status | open → fixed / exception / rejected |
| task | the tasks.md id that fixes it |

## Exception (`docs/accessibility.md` → Known exceptions)

| Field | Rules |
|---|---|
| what | the element or page |
| criterion | the WCAG SC |
| reason | why it's accepted |
| plan | fix or review date |

## Announcement (runtime, `web/src/lib/a11y.ts`)

`announce(text)`: polite. The same text within 2 s is dropped. Sources:
- a verdict change;
- a new alert;
- connection lost or restored.
