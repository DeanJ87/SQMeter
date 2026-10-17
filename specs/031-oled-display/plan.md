# Implementation Plan: OLED display

**Branch**: `feature/travel-allsky` (spec branch `spec/031-oled-display`) | **Date**: 2026-10-17 |
**Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

## Summary

Drive an SSD1306 128 × 64 OLED at 0x3C on the main I2C bus with a small in-house transport
(`src/OledDisplay.cpp`) and do everything else in a new pure library, `lib/DisplayLogic`: page
rendering into a 1 KB framebuffer with two built-in fonts, the power/timeout/night/page-cycle state
machine, detection back-off and button debounce, all natively tested. `lib/DeviceCore` builds the
page view model from the same readings snapshot every interface uses, so stale or faulted values
become `--` by the existing status rules. The firmware sends one dirty framebuffer page per loop pass,
so the loop never blocks. At night, or whenever night can't be computed, the display is dim, minimal
and always times out unless the owner chooses to stay on at a known night. Config is additive (display
off by default); `/api/status` reports `display`; two authenticated routes wake and blank it; Settings
gains a Display card.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5, pioarduino); TypeScript strict (Preact + Vite); Python 3 tools

**Primary Dependencies**: Arduino `Wire` only (no display library, research D1); ArduinoJson 6; Zod

**Storage**: NVS main config JSON (5,100 B limit): `display` section ~+200 B (research D8)

**Testing**: Unity native tests (`pio test -e native`, new `test/test_display_logic`), Vitest + MSW,
contract schemas (spec 016), OpenAPI check, settings-deps cases, ConformU unchanged

**Target Platform**: ESP32 classic today (any spare I2C port); ESP32-S3 Rev A boards (OLED port J6,
AUX pins) once board profiles land; browser demo (WASM core)

**Project Type**: embedded firmware + embedded web UI + docs

**Performance Goals**: value on screen within one read interval of the reading (FR-006); at most one
~12 ms I2C page write per loop pass at 100 kHz (research D2)

**Constraints**: OTA slot (SIZE-04; BLE build has ~52 KB to the 95% fail line, shared with spec 032:
target ≤ 9 KB here), framebuffer heap only when enabled (FR-016), loop stack headroom > 2 KB, UI gzip
growth < 10 KB, config < 5,100 B, no bus traffic when disabled

**Scale/Scope**: 1 display, 7 page types; ~14 firmware/lib files, ~10 web files, 6 docs pages

## Constitution Check

*Gate before Phase 0; re-checked after Phase 1 (below).*

| Principle | How this plan satisfies it | Status |
|---|---|---|
| I Fail-safe verdict | The display only reads the verdict; nothing it does feeds back. Stale/faulted values show `--` via `Readings` status (FR-005) | Pass |
| II Alpaca conformance | No Alpaca change | Pass (n/a) |
| III Testable pure logic | Rendering, page selection, controller, link back-off, debounce in `lib/DisplayLogic`; view model in `lib/DeviceCore`; config/deps in their libs; all native-tested; UI in Vitest | Pass |
| IV Resource budgets | Own driver (D1), dirty-page transfer (D2), framebuffer allocated only when enabled, sizes and stack measured into budgets.md; handlers only set atomic requests | Pass |
| V Quiet UI | One Settings card from shared controls, reasons via settings deps, no dashboard card, mockup reviewed first (contracts/ui.md) | Pass (mockup review is a task gate) |
| VI Trusted-LAN security | Wake/blank routes `requireAuth`; no secrets | Pass |
| VII Docs with behaviour | New `docs/user-guide/display.md` (pages, labels, night, wake), hardware wiring/port, configuration, REST, settings-dependencies in the same change (FR-018) | Pass |
| VIII Code quality | `tools/quality/check.py` incl. DASH-01/02, DS, settings-deps, i18n; no baseline growth | Pass |
| Compatibility | Additive config, display off when missing; new status/route fields only | Pass |

## Project Structure

### Documentation (this feature)

```text
specs/031-oled-display/
├── spec.md
├── plan.md              # this file
├── research.md          # D1-D13
├── data-model.md
├── quickstart.md
├── budgets.md           # filled during implementation
├── contracts/
│   ├── config.md
│   ├── status-and-routes.md
│   ├── screen.md
│   └── ui.md
├── mockups/             # owner review before the Settings card is built
└── tasks.md
```

### Source Code (repository root)

```text
lib/DisplayLogic/include/DisplayTypes.h      Page, Brightness, names/parse
lib/DisplayLogic/include/DisplayFrame.h      Frame (framebuffer, dirty pages), fonts, render(Page, View, night, shift)
lib/DisplayLogic/include/DisplayController.h Controller (on/off, timeout, night, cycle, shift), Link, Button
lib/DisplayLogic/src/*.cpp                   (no Arduino headers)
lib/ConfigModel/                             DisplayConfig, JSON in/out, validation, pin checks
lib/DeviceCore/                              Core::displayView, status "display", Facts.displayDetected
lib/SettingsDeps/ + catalogue.json           D-43..D-45, reason display-missing
include/OledDisplay.h, src/OledDisplay.cpp   SSD1306 transport: probe, init, contrast, on/off, remap, page write
src/main.cpp                                 create/tick the display after the sensors; button sampling
include/WebServer.h, src/WebServerApi.cpp, src/WebServerStatus.cpp   routes, status, view handoff
platformio.ini                               -Ilib/DisplayLogic/include for native
tools/demo-core/build.sh, emulated_device*.cpp  DisplayLogic in the WASM core; demo display status + routes
tools/api/routes.json, operations.json, docs/api/openapi.json
specs/016-demo-device-emulation/contracts/schemas/{config,status,settings-effective}.schema.json
test/test_display_logic/, test_config_model, test_config_migration, test_device_core, test_settings_deps,
test/fixtures (config-ranges, settings-deps default-config + cases, config-releases)
web/src/types/{config,status}.ts, validation/configSchema.ts, components/settings/{DisplayCard.tsx,DeviceTab.tsx,defaults.ts,payload.ts},
web/src/components/System.tsx, lib/settingsDeps.ts, hooks/useDeviceActions.ts, mocks/, demo/, dashboard/inventory.json, i18n/
docs/user-guide/display.md (new), docs/getting-started/hardware.md, docs/hardware/overview.md,
docs/user-guide/configuration.md, docs/api/rest.md, docs/reference/settings-dependencies.md, mkdocs.yml
```

**Structure Decision**: the existing layout plus one new library, `lib/DisplayLogic`, for the pure
display logic; the firmware glue is one new transport file pair.

## Phases

1. **Foundation**: `DisplayTypes`, config model + validation + migration, settings-deps entries (tests first).
2. **Logic (US1-US4 core)**: frame + fonts + page renderers, controller, link, button (tests first);
   `Core::displayView` and status.
3. **Firmware**: SSD1306 transport, `main.cpp` wiring, routes, status; both builds; budgets.
4. **UI**: mockup and owner review, Display card, System row, strings and translations.
5. **Demo, contracts, docs, budgets**: WASM core, schemas, OpenAPI, docs pages, quality gate,
   device checks on the spare device.

## Post-design Constitution re-check

The Phase 1 artifacts keep every gate: config and status are additive; no Alpaca or safety path
changes; the UI contract uses only shared components and settings dependencies; every decision that
isn't I/O is in `lib/`. No violations, so no Complexity Tracking entries.

## Complexity Tracking

None.
