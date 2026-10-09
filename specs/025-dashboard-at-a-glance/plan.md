# Implementation Plan: Dashboard at a glance

**Branch**: `feat/025-dashboard-at-a-glance` | **Spec**: [spec.md](spec.md)

## Summary

An at-a-glance area at the top of the dashboard that always shows freshness, the safety verdict and whether alerts will go out, plus every active problem (imaging app stopped checking, failed sensor, settings not in effect, clock or location unknown, phone alarm). Sensor cards stay in a fault state instead of vanishing. A machine-readable inventory maps every device field and settings dependency to where it is shown (or why not), and a check plus per-item demo tests keep it from regressing.

## Technical Context

- **Language/Version**: TypeScript (Preact, Vite) for the UI; C++17 (lib/RainLogic, lib/Readings) for one device field; Python 3 (standard library) for the check.
- **Testing**: Vitest (glance logic), Playwright against the demo (inventory items), native tests (rain hold), unittest (check).
- **Constraints**: device UI bundle ≤ +4 KB gzip (SC-007); firmware change minimal; translations for every new string (spec 023); WCAG 2.2 AA (spec 022); 320 px.
- **Research**: [research.md](research.md) D1–D12. No open questions.

## Constitution Check

| Principle | How |
|---|---|
| I Fail-safe | A failed sensor stays visible; unknown is shown as unknown where safety-relevant; nothing shows "OK" for missing data. |
| III Testable logic | `glanceItems()` pure and unit-tested; rain hold remaining time in lib/RainLogic with native tests. |
| IV Budgets | +4 KB gzip UI; one integer field on the device. |
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
