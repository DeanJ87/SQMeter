# Implementation Plan: Firmware and Web UI Updates

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/012-ota-updates/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`OtaUpdater` fetches the GitHub releases list (filtered, streamed JSON, document allocated before
TLS, TlsLock), `parseGithubReleases` selects track and assets (BLE prefix on the BLE build), and
apply streams filesystem then firmware. `/api/update` and `/api/update/fs` take manual uploads.
The release workflow injects the tag version into `include/version.h` and publishes assets.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: Update (ESP-IDF OTA), WiFiClientSecure (pinned GitHub roots), ArduinoOTA
**Testing**: Vitest (`versionCompare.test.ts`, Updates); no native tests for release parsing
**Target Platform**: ESP32; GitHub Actions
**Project Type**: Embedded firmware + CI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| III. Testable logic | Release selection tested | Browser version compare tested; device parseGithubReleases untested |
| IV. Budgets | Fits slots; heap for TLS | Yes |
| Platform: atomic OTA | Failure keeps previous firmware | Yes |
| VII. Docs | Labels current | docs/user-guide/ota.md |

## Project Structure

### Documentation (this feature)

```text
specs/012-ota-updates/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/OtaUpdater.cpp, include/OtaUpdater.h, include/GithubRootCA.h
src/WebServer.cpp                 # /api/update, /api/update/fs, /api/updates/check, /api/updates/apply
include/version.h                 # FIRMWARE_VERSION (0.0.2 outside CI)
web/src/components/Updates.tsx, web/src/utils/versionCompare.ts
.github/workflows/build.yml       # version injection, release assets, prerelease flag
docs/user-guide/ota.md, docs/getting-started/flashing.md
```

**Structure Decision**: Firmware + CI release pipeline.

## Complexity Tracking

None.
