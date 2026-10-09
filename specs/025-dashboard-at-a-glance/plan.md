# Implementation Plan: Dashboard at a glance

**Branch**: `feat/025-dashboard-at-a-glance` | **Spec**: [spec.md](spec.md)

## Summary

An at-a-glance area at the top of the dashboard that always shows freshness, the safety verdict and whether alerts will go out, plus every active problem (imaging app stopped checking, failed sensor, settings not in effect, clock or location unknown, phone alarm). Sensor cards stay in a fault state instead of vanishing. A machine-readable inventory maps every device field and settings dependency to where it is shown (or why not), and a check plus per-item demo tests keep it from regressing.

## Technical Context

- **Language/Version**: TypeScript (Preact, Vite) for the UI; C++17 (lib/RainLogic, lib/Readings) for one device field; Python 3 (standard library) for the check.
- **Testing**: Vitest (glance logic), Playwright against the demo (inventory items), native tests (rain hold), unittest (check).
- **Constraints**: device UI bundle ≤ +10 KB gzip (SC-007, SIZE-01); firmware change minimal; translations for every new string (spec 023); WCAG 2.2 AA (spec 022); 320 px.
- **Research**: [research.md](research.md) D1–D12. No open questions.

## Constitution Check

| Principle | How |
|---|---|
| I Fail-safe | A failed sensor stays visible; unknown is shown as unknown where safety-relevant; nothing shows "OK" for missing data. |
| III Testable logic | `glanceItems()` pure and unit-tested; rain hold remaining time in lib/RainLogic with native tests. |
| IV Budgets | +10 KB gzip UI (SIZE-01); one integer field on the device. |
| V Quiet UI | Healthy state is one line; detail stays in cards. |
| VII Docs | Dashboard guide describes every inventory item; REST docs for `clearInSeconds`; diagrams checked. |
| VIII Quality | DASH-01 (human) and DASH-02 (auto) rules; check in the quality gate; no baseline. |

## Project Structure

```
web/src/dashboard/inventory.json      inventory (D1)
web/src/dashboard/glance.ts           glanceItems() (D4) + __tests__
web/src/dashboard/AtAGlance.tsx       the area at the top
web/src/components/Dashboard.tsx      fault cards (D6), Device & Network additions, glance
web/src/components/SafetyCard.tsx     rules not in effect, rain hold
web/src/lib/demoInfo.ts               demo marker (D11)
web/tests/dashboard.spec.ts           one test per shown inventory entry
tools/dashboard/check.py (+ test)     the check (D3)
lib/RainLogic, lib/Readings            clearInSeconds (D7)
```

## Phases

1. Device field (D7) and schema; demo core rebuilt.
2. Inventory and check (D1–D3), wired into the quality gate and build.
3. `glanceItems()` and the at-a-glance area; fault cards; safety card additions; Device & Network additions; demo marker.
4. Translations for every new string (13 languages).
5. Playwright tests per inventory entry; docs (dashboard guide, REST, coding standard, constitution); budgets; converge.

## Budgets (T019)

Measured against `main` at bb907ed, gzip level 9, every `.js` and `.css` in `web/dist` (the generated `mockServiceWorker.js` excluded; it is demo-only):

| File | main | this branch | Delta |
|---|---:|---:|---:|
| App chunk | 69,491 | 73,021 | +3,530 |
| English strings chunk | 28,370 | 29,364 | +994 |
| CSS | 7,720 | 8,056 | +336 |
| **Total** | **105,581** | **110,441** | **+4,860** |

**SC-007 is met**: +4.75 KB against the 10 KB budget. The budget was 4 KB when this was built; it was raised to 10 KB per feature (coding standard SIZE-01) on 2026-10-09, after a review of the device filesystem (the UI is now stored gzipped, ~150 KB of the 512 KB partition). Already trimmed: eight strings that repeated existing keys now reuse them, and the item builders share one helper; neither moved the gzip size much, because gzip was already absorbing the repetition. What remains is new behaviour: the glance logic (~5.4 KB minified), the Device & Network additions (IPv6, `.local` name, MQTT state, update available; ~2.8 KB minified), the area itself, and 53 new strings. Getting under 4 KB would mean dropping a required item rather than trimming.

Firmware (`esp32dev`): flash 1,483,677 → 1,483,969 bytes (+292), RAM unchanged (55,768).

## Converge (T020)

- FR-001–FR-004: inventory, check (DASH-02 in the quality gate and CI), per-entry tests, DASH-01 in the coding standard and the constitution - done.
- FR-005–FR-007, FR-014: area at the top, priority order, problems with consequence and fix - done; FR-020 / SC-004 covered by the healthy-line test at 1280 and 320 px.
- FR-008: the sentence and since-time come from spec 021's `describeSchedule`; Resume is on the paused item. Pause while sending stays on the alerts bell in the header, which is on every page, so the healthy line keeps no control of its own.
- FR-009–FR-013, FR-015–FR-018, FR-021, FR-022: done, each with an inventory entry and a test (phone alarm in a unit test: the demo is the standard build).
- FR-019: all text through `t()`, in 14 languages; announcements only for urgent items.
- FR-023: dashboard guide describes every item; screenshots are regenerated by the screenshot test in the docs build; diagrams checked.
- SC-007: met (+4.75 KB against 10 KB), see Budgets.
