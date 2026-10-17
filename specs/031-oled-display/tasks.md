# Tasks: OLED display

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: required by constitution III (native Unity tests for `lib/*` logic, Vitest for UI logic).
Test tasks come before the code they cover and must fail first.

**Format**: `- [ ] T### [P?] [US?] Description with file path`. `[P]` = different files, no
dependency on an unfinished task.

## Phase 1: Setup

- [X] T001 Record the baselines in specs/031-oled-display/budgets.md: firmware size of `pio run -e esp32dev` and `-e esp32dev-ble`, static RAM, gzipped UI JS+CSS (`tools/ui/size_check.py`), default and long-string main config JSON size, `/api/status` worst case (`test_status_document_fits`)
- [X] T002 Create lib/DisplayLogic/ (include/, src/, like lib/WindLogic) and add `-Ilib/DisplayLogic/include` to `[env:native]` build_flags in platformio.ini; add `lib/DisplayLogic/src/*.cpp` to SOURCES in tools/demo-core/build.sh
- [X] T003 [P] `Display::Page` enum (Sky, Environment, Cloud, Safety, Gps, Battery, Network), `PAGE_COUNT`, `pageName()`/`parsePage()` (exact names), `Brightness` (Low/Medium/High) with names/parse in lib/DisplayLogic/include/DisplayTypes.h, with tests first in test/test_display_logic/test_display_types.cpp

## Phase 2: Foundational (blocks every story)

### Config model (research D8, contracts/config.md)

- [X] T004 [P] Native tests first in test/test_config_model/: `display` defaults (enabled false, pages `["sky"]`, cycleSeconds 5, brightness medium, timeoutSeconds 60, wakeOnBoot true, stayOnAtNight false, rotate180 false, buttonPin 255); JSON round trip incl. page order; every validation message in contracts/config.md (pages 1..7 unique known names, unknown name rejected; cycle 2..60; brightness names; timeout 0 or 10..1800; buttonPin 255 or valid GPIO; buttonPin clash with I2C-1, I2C-2, GPS, rain, wind speed/vane); partial update keeps other display keys; worst-case main JSON (long strings, both sky sensors, display with 7 pages) < `Config::MAX_PERSISTED_JSON_BYTES`
- [X] T005 [P] Migration test first in test/test_config_migration/: a pre-031 saved config fixture in test/fixtures/config-releases/ loads with every existing value kept and the display off with defaults (FR-017)
- [X] T006 Add `DisplayConfig display` (pages as `uint8_t pages[PAGE_COUNT]` + `pageCount`) to lib/ConfigModel/include/Config.h; defaults in lib/ConfigModel/src/ConfigModel.cpp
- [X] T007 JSON in/out for `display` in lib/ConfigModel/src/ConfigJsonIn.cpp and lib/ConfigModel/src/ConfigJsonOut.cpp (pages via `Display::parsePage`, rejected when unknown or duplicated)
- [X] T008 Validation in lib/ConfigModel/src/ConfigValidate.cpp: ranges and messages exactly as contracts/config.md; a `validateDisplayPins` that uses `busPins()` and the wind pins (and, once spec 032 lands, the battery pins); T004/T005 pass
- [X] T009 [P] Range and default fixture entries for `display.*` in test/fixtures/config-ranges.json and test/fixtures/settings-deps/default-config.json (test/test_config_ranges)

### Settings dependencies (research D11, contracts/config.md)

- [X] T010 Native tests first in test/test_settings_deps/ and cases in test/fixtures/settings-deps/cases.json: D-43 (display settings inactive with `display-missing` when `Facts.displayDetected` is false and the display is enabled; off when disabled), D-44 (`display.stayOnAtNight` inactive with `location-unknown`), D-45 constraint messages; `display.enabled` in `noDependency`
- [X] T011 Catalogue entries D-43..D-45 and reason `display-missing` ("No display detected", fix `device#display`) in lib/SettingsDeps/catalogue.json; `Facts.displayDetected` and the entries in lib/SettingsDeps/include/SettingsDeps.h and lib/SettingsDeps/src/SettingsDeps.cpp; mirror in web/src/lib/settingsDeps.ts; `tools/settings-deps/check.py` and T010 pass

### Display logic core (research D1, D2, D4, data-model)

- [X] T012 [P] Native tests first in test/test_display_logic/test_frame.cpp: `Frame` clear/set pixel, 5×7 text with 6 px advance, 10×16 digits, ellipsis when text exceeds the width, dirty bits only on changed pages, pixel shift offsets for positions 0..3
- [X] T013 `Frame` (1,024-byte buffer in SSD1306 page layout, dirty mask), fonts (5×7 ASCII 0x20-0x7E plus `°` and `…`; 10×16 digits, `.`, `-`, `%`) in lib/DisplayLogic/include/DisplayFrame.h, lib/DisplayLogic/src/DisplayFrame.cpp and lib/DisplayLogic/src/DisplayFonts.cpp (fonts `const` in flash); T012 passes
- [X] T014 [P] Native tests first in test/test_display_logic/test_link.cpp: `Link` probe when enabled, re-probe every 5 s while not detected, 3 consecutive write failures → not detected, `needsInit` after (re)detection, nothing while disabled
- [X] T015 `Link` and `Button` (30 ms debounce, one press per press) in lib/DisplayLogic/include/DisplayController.h and lib/DisplayLogic/src/DisplayLink.cpp; T014 passes

**Checkpoint**: config, deps and display logic compile natively; existing native tests unchanged and green.

## Phase 3: User Story 1 - Read the sky at a glance (P1) MVP

**Goal**: with an OLED fitted and enabled, the Sky page shows the authoritative reading and `--` when it
is missing or stale; without an OLED nothing changes.
**Independent test**: quickstart sections 3 and 4.1-4.2.

### Tests

- [X] T016 [P] [US1] Native tests first in test/test_display_logic/test_pages.cpp: Sky page day layout per contracts/screen.md (name `SQM`/`SQM-L`, large value, Bortle, NELM, the other sky reading in small type only when present and valid); invalid sky → `--` in place of every value, never a number
- [X] T017 [P] [US1] Native tests first in test/test_device_core/: `Core::displayView` from a `SensorSnapshot` + `Readings::Snapshot`: sky group follows the authoritative role and `Readings` status (ok → values; missing/error/stale → invalid), other sky reading only when its entry is ok; page availability rules from research D4; `/api/status` `display` object for enabled/disabled/detected/not detected (contracts/status-and-routes.md); `test_status_document_fits` still passes

### Implementation

- [X] T018 [US1] `Display::View` and `render(Page::Sky, ...)` in lib/DisplayLogic/include/DisplayFrame.h and lib/DisplayLogic/src/DisplayPages.cpp; T016 passes
- [X] T019 [US1] `Core::displayView`, `Core::writeDisplayStatus` and `Facts.displayDetected` filling in lib/DeviceCore/include/DeviceCore.h and lib/DeviceCore/src/DeviceDisplay.cpp; T017 passes
- [ ] T020 [US1] SSD1306 transport: probe (ACK + one-byte status read), init sequence, contrast, display on/off, segment/COM remap for 180°, page write (129-byte transaction), in include/OledDisplay.h and src/OledDisplay.cpp (Arduino `Wire` only)
- [ ] T021 [US1] src/main.cpp: create the display after `setupSensors()` only when `display.enabled` (framebuffer `std::unique_ptr` allocated then, none otherwise); in `loop()` tick `Link`/`Controller`, render on each `refreshSensorSnapshot` and page change, send at most one dirty page per pass; reconfigure on settings save in `saveConfigCallback` (enable/disable, rotation, brightness) with no restart
- [ ] T022 [US1] WebServer: hand the display a view after each `refreshSensorSnapshot` (`WebServer::displayView()` under the snapshot mutex), add `display` to `/api/status` and `/ws/status`, `displayDetected` to `settingsFacts`, in include/WebServer.h, src/WebServer.cpp and src/WebServerStatus.cpp
- [ ] T023 [US1] Both firmware builds pass with `-Werror` (`pio run -e esp32dev`, `-e esp32dev-ble`); record sizes in specs/031-oled-display/budgets.md (target ≤ 9 KB growth on the BLE build)

**Checkpoint**: US1 works on the device with the default single Sky page; a device without a display
reports `display.detected: false` and is otherwise unchanged.

## Phase 4: User Story 3 - Protect dark adaptation (P1)

**Goal**: at night (or night unknown) the display is dim, minimal and times out unless the owner keeps it
on at a known night.
**Independent test**: quickstart section 5.

### Tests

- [X] T024 [P] [US3] Native tests first in test/test_display_logic/test_controller.cpp: wake on boot for the timeout; off at the timeout; timeout 0 by day stays on; at night timeout 0 → 60 s; unknown night behaves as night; `stayOnAtNight` keeps it on only when night is known; night mode → contrast 0x00 and `nightMode` true; day contrast low/medium/high = 0x10/0x7F/0xFF; disabled or not detected → off; millis wrap
- [X] T025 [P] [US3] Native tests first in test/test_display_logic/test_pages.cpp: night layout of every page (one large value, no inverse pixels or borders) per contracts/screen.md

### Implementation

- [X] T026 [US3] `Controller` (on/off, effective timeout, night mode, contrast, rotation pass-through) in lib/DisplayLogic/include/DisplayController.h and lib/DisplayLogic/src/DisplayController.cpp; T024 passes
- [X] T027 [US3] Night layouts in lib/DisplayLogic/src/DisplayPages.cpp; T025 passes
- [ ] T028 [US3] Feed `Core::night(...)` (via `WebServer::computeNight`) into the controller each tick and apply contrast/VCOMH and on/off through the transport in src/main.cpp and src/OledDisplay.cpp

**Checkpoint**: with default settings the display emits no light after dusk beyond the timeout (SC-002).

## Phase 5: User Story 2 - Pages for everything else (P2)

**Goal**: the owner picks pages, order and interval; pages without a data source are skipped.
**Independent test**: quickstart section 4.3-4.5.

### Tests

- [X] T029 [P] [US2] Native tests first in test/test_display_logic/test_pages.cpp: day layouts for Environment, Cloud, Safety (first reason cut with `…`, inverse block when unsafe by day), GPS, Battery, Network (WiFi IP / AP name + address / no connection) per contracts/screen.md; invalid values → `--`
- [X] T030 [P] [US2] Native tests first in test/test_display_logic/test_controller.cpp: cycling through selected ∩ available pages in configured order every `cycleSeconds`; one visible page doesn't cycle; none visible → Network; pixel shift advances on each page change; a page becoming unavailable is skipped on the next change
- [X] T031 [P] [US2] Native tests first in test/test_device_core/: `displayView` groups environment, cloud, safety (verdict + first reason), gps, network (device name, mode, IP, AP name) and their availability; battery group unavailable (filled by spec 032)

### Implementation

- [X] T032 [US2] Page renderers for Environment, Cloud, Safety, GPS, Battery, Network in lib/DisplayLogic/src/DisplayPages.cpp; T029 passes
- [X] T033 [US2] Page cycling and shift in lib/DisplayLogic/src/DisplayController.cpp; T030 passes
- [ ] T034 [US2] View groups in lib/DeviceCore/src/DeviceDisplay.cpp; network facts (`isConnected`, `isInAPMode`, IP, AP name) passed from `WiFiManager` by src/main.cpp / src/WebServer.cpp; T031 passes

## Phase 6: User Story 4 - Wake without a button (P3)

**Goal**: wake from the web UI or API, and from an optional button.
**Independent test**: quickstart section 6.

### Tests

- [X] T035 [P] [US4] Native tests first in test/test_display_logic/test_controller.cpp: wake request turns on for the timeout; blank turns off until the next wake; button press while off wakes, while on advances the page and restarts the timeout; requests ignored while disabled/not detected/forced off
- [ ] T036 [P] [US4] Vitest first for the wake action (`POST /api/display/wake`, success and each 409 message) in web/src/components/__tests__/DeviceTab.test.tsx

### Implementation

- [X] T037 [US4] Wake/blank/button handling in lib/DisplayLogic/src/DisplayController.cpp; T035 passes
- [ ] T038 [US4] Routes `POST /api/display/wake` and `/api/display/blank` with `requireAuth`, atomic request flags applied by the loop task, 409 messages from contracts/status-and-routes.md, registered before broader routes, in src/WebServerApi.cpp and include/WebServer.h; entries in tools/api/routes.json and tools/api/operations.json
- [ ] T039 [US4] Wake button: `pinMode(INPUT_PULLUP)` when `buttonPin` != 255, sampled every loop into `Button`, in src/main.cpp

## Phase 7: UI (US1-US4 settings)

- [ ] T040 Mockup of the Settings → Device → Display card (detected, not detected, disabled, stay-on note, validation error) and the System Sensors row from contracts/ui.md in specs/031-oled-display/mockups/ (390 px and 1280 px) with a README; owner review recorded before T042-T044 are built
- [ ] T041 [P] Web types and mocks: `DisplayConfig` in web/src/types/config.ts, `Status.display` in web/src/types/status.ts, Zod in web/src/validation/configSchema.ts, defaults in web/src/components/settings/defaults.ts, payload in web/src/components/settings/payload.ts, MSW handlers for `/api/display/wake|blank` and status in web/src/mocks/
- [ ] T042 Vitest first for the Display card states (detected, not detected with the D-43 reason, disabled, stay-on note, one page selected disables the interval, validation error) in web/src/components/settings/__tests__/DisplayCard.test.tsx; then the card per contracts/ui.md in web/src/components/settings/DisplayCard.tsx, added to web/src/components/settings/DeviceTab.tsx after Bluetooth; wake action in web/src/hooks/useDeviceActions.ts (T036 passes); `device#display` anchor works from settings-deps fix links
- [ ] T043 System page "Display" row in the Sensors card in web/src/components/System.tsx with a Vitest in web/src/components/__tests__/System.test.tsx
- [ ] T044 Dashboard inventory: `status.display.*` and the D-43..D-45 ids recorded as not shown with reason in web/src/dashboard/inventory.json; `tools/dashboard/check.py` passes

## Phase 8: Polish & cross-cutting

### Demo and contracts (FR-014)

- [ ] T045 [P] Contract schemas: `display` in specs/016-demo-device-emulation/contracts/schemas/status.schema.json, config keys in config.schema.json, `displayDetected` in settings-effective.schema.json; no existing property changed
- [ ] T046 Demo: display detected in the demo device, wake/blank routes, controller-driven `display.on`/`page`, in tools/demo-core/emulated_device.cpp, tools/demo-core/emulated_device_documents.cpp and web/src/demo/handlers.ts; rebuild the WASM core with tools/demo-core/build.sh (or note emsdk unavailable); `tools/contract-check.py` passes
- [ ] T047 `python tools/api/openapi.py` regenerates docs/api/openapi.json; `--check` passes

### Strings (FR-019)

- [ ] T048 Every new UI string in web/src/i18n/en.json with a context note in web/src/i18n/en.context.json; translations written for all 13 locales in web/src/i18n/locales/*.json in each file's register; device-built messages (validation, route errors, `display-missing`) via `python3 tools/i18n/gen_device_catalog.py`; `python3 tools/i18n/translate.py --all --record`, `node tools/i18n/check.mjs`, `gen_device_catalog.py --check` pass (no paid API)

### Docs (FR-018, constitution VII)

- [ ] T049 [P] New docs/user-guide/display.md: fitting, enabling, pages, the full on-screen label list (contracts/screen.md), night behaviour incl. unknown night, wake (web, API, button), 0x3C clash note; nav entry in mkdocs.yml
- [ ] T050 [P] Hardware: the OLED port (J6) and wiring on any board's spare I2C port, AUX pins for the button, in docs/getting-started/hardware.md and docs/hardware/overview.md
- [ ] T051 [P] docs/user-guide/configuration.md (`display` keys, defaults, validation), docs/api/rest.md (status `display`, wake/blank routes), docs/api/websocket.md (status), docs/reference/settings-dependencies.md (D-43..D-45, regenerated where generated); Settings paths match en.json labels; `python3 -m mkdocs build --strict` where mkdocs is installed

### Budgets and gates (FR-016, constitution IV, VIII)

- [ ] T052 Measure after: firmware size both builds (SIZE-04), static RAM, UI gzip delta ≤ 10 KB (SIZE-02), config and status worst cases (tests); update specs/031-oled-display/budgets.md; device heap and loop `stackFree` with and without a display on the spare device only, noted for the PR
- [ ] T053 Full gate: `pio test -e native`, both firmware builds, `tools/quality/check.py` (no new findings), `tools/api/openapi.py --check`, web `vitest run`, `tsc --noEmit`, `eslint src`, i18n checks, settings-deps and dashboard checks; ConformU unchanged (no Alpaca change; CI runs it)
- [ ] T054 Run quickstart.md sections 1 and 2 and record results; sections 3-6 on the spare device only, with the owner's permission (never the main device), or recorded as open for the PR; changelog entry in CHANGELOG.md
- [ ] T055 Converge (`speckit-converge`) until it reports converged

## Dependencies

- Phase 1 → Phase 2 → stories. T006 before T007/T008; T013 before T018; T015 before T021.
- US1 (Phase 3) is the MVP; US3 needs T018-T022 (rendering and wiring) and adds the controller;
  US2 needs T026 (controller) for cycling; US4 needs T026 and T038's routes.
- T040 (mockup review) gates T042-T044.
- Spec 032 later fills the Battery page (view group) and adds the battery pins to T008's pin check
  and a `forcedOff` input (power saving).
- Polish after the stories it documents; T046 after every `lib/*` change; T055 last.

## Parallel examples

- Phase 2: T004, T005, T009, T012, T014 together (different test dirs/fixtures).
- US1: T016 and T017 together; T020 alongside T018/T019.
- US3: T024 and T025 together.
- US2: T029, T030, T031 together.
- Polish: T045 and T049-T051 together.

## Implementation strategy

1. MVP = Phases 1-3 (US1): the Sky page on the device, nothing changes without a display.
2. US3 next (also P1): night safety before more pages make the screen more tempting to leave on.
3. US2 (pages) and US4 (wake) in either order; the UI phase after the mockup review.
4. Polish: demo, schemas, translations, docs, budgets, convergence.
