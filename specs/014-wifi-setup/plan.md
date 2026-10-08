# Implementation Plan: WiFi Setup and Network Presence

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/014-wifi-setup/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

`WiFiManager` joins the saved network with back-off, or starts the "SQM-Setup" soft AP with a
wildcard DNS server. The web server redirects OS captive-portal probes to `/`. WiFi is configured
in Settings → Network (scan + select + save); `/api/wifi/scan` and `/api/wifi/connect` exist.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: WiFi, DNSServer
**Storage**: NVS (`wifi`)
**Testing**: Vitest (Settings/Network)
**Target Platform**: ESP32; browser
**Project Type**: Embedded firmware + web UI

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| VI. Security | WiFi password masked; connect needs auth | Yes |
| VII. Docs | First-boot guide matches | docs/getting-started/first-setup.md |

## Project Structure

### Documentation (this feature)

```text
specs/014-wifi-setup/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/WiFiManager.cpp, include/WiFiManager.h   # station/AP, DNS, back-off
src/WebServer.cpp                            # captive-portal probe redirects, /api/wifi/scan, /api/wifi/connect
web/src/components/settings/NetworkTab.tsx   # WiFi card
docs/getting-started/first-setup.md, docs/user-guide/configuration.md, docs/hardware/rg15.md (example URLs)
```

**Structure Decision**: Single firmware project with web UI in `web/`.

## Complexity Tracking

None.
