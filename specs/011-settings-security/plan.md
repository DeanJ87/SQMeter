# Implementation Plan: Settings, Configuration and Security

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/011-settings-security/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`Settings.tsx` hosts six tab components built from shared controls; `configSchema.ts` (Zod)
validates before saving; `restart.ts` decides restart prompts; `tabs.ts` maps sections and error
paths to tabs. The device's `Config::validate`, JSON load/save (main key + separate alerts key) and
migrations live in `src/Config.cpp`; `requireAuth` guards routes in `src/WebServer.cpp`.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact, Zod)
**Storage**: NVS (`config` ≤ 5100 B, `alerts` ≤ 3900 B)
**Testing**: Vitest (Settings, settingsLogic, configSchema), Unity
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| IV. Budgets | NVS limits validated | Yes |
| V. Quiet UI | Shared controls, tooltips, toasts | Yes |
| VI. Security | requireAuth everywhere; secrets masked | Yes (18 protected endpoints) |
| VII. Docs | Config and security docs match | configuration.md, security.md |

## Project Structure

### Documentation (this feature)

```text
specs/011-settings-security/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
web/src/components/Settings.tsx, web/src/components/settings/*.tsx|ts (tabs, controls, restart, payload, defaults)
web/src/validation/configSchema.ts
src/Config.cpp, include/Config.h
src/WebServer.cpp (requireAuth, /api/config)
src/main.cpp (saveConfigCallback live updates)
docs/user-guide/configuration.md, docs/user-guide/security.md
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
