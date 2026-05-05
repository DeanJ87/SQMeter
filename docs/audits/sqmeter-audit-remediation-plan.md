# SQMeter Audit Remediation Plan

This file tracks closure for the findings in `docs/audits/sqmeter-nuclear-codebase-audit.md`.

## Integrated Workstreams

| Workstream | Commit | Status |
|------------|--------|--------|
| Security/secrets/OTA | `f8244b6` | Merged |
| Config/API/settings | `b81d313` | Merged |
| Firmware async/reliability | `26c0c92` | Merged |
| MQTT/telemetry/ASCOM | `6511961` | Merged |
| UI/MSW/UX | `0337c3f` | Merged |
| Tests/CI/build | `7fa2f21` | Merged |
| Docs/remediation | `09aaa43` | Merged |
| Architecture cleanup | `d11421d` partial | Cherry-picked non-conflicting `Logger.cpp` and `TSL2591Sensor.cpp` changes |

## Critical And High Findings

| ID | Severity | Status | Branch/commit | Files | Validation evidence | Follow-up |
|----|----------|--------|---------------|-------|---------------------|-----------|
| AUDIT-001 | Critical | Fixed | `fix/audit-firmware-async-state` `26c0c92` | `src/WebServer.cpp`, `include/WebServer.h`, `src/main.cpp`, `src/sensors/RG15Sensor.cpp` | Worker `pio run` passed; final `rg "delay\\("` review required | MQTT test remains synchronous; consider a nonblocking MQTT test state machine |
| AUDIT-002 | Critical | Fixed | `fix/audit-firmware-async-state` `26c0c92` | `src/WebServer.cpp`, `include/WebServer.h`, `src/main.cpp` | Worker `pio run` passed; final firmware build required | Hardware/soak test under simultaneous WebSocket/API/MQTT load |
| AUDIT-003 | Critical | Fixed | `fix/audit-security-secrets-ota` `f8244b6`, config `b81d313` | `src/Config.cpp`, `include/Config.h` | Secret logging grep required after final merge | Keep serial logs reviewed before release |
| AUDIT-004 | High | Fixed | `fix/audit-config-api-settings` `b81d313`, UI `0337c3f` | `src/WebServer.cpp`, `web/src/components/Settings.tsx`, `web/src/mocks/handlers.ts` | Config branch firmware/web builds passed; final web build required | None |
| AUDIT-005 | High | Fixed | `fix/audit-telemetry-mqtt-ascom` `6511961` | `src/MQTTClient.cpp`, `include/MQTTClient.h` | Worker `pio run` passed | Live broker test with two devices |
| AUDIT-006 | High | Fixed | `fix/audit-firmware-async-state` `26c0c92` | `src/WebServer.cpp`, `include/WebServer.h`, UI/docs updates | Worker `pio run` passed; final `rg "WiFi\\.scanNetworks\\("` review required | Confirm UI handles `202` scan-in-progress payload |
| AUDIT-007 | High | Fixed | `fix/audit-config-api-settings` `b81d313` | `src/Config.cpp`, `include/Config.h` | Config branch `pio run` passed; final `rg serializeJsonPretty` required | Add firmware unit test for oversized config |
| AUDIT-008 | High | Fixed | `fix/audit-security-secrets-ota` `f8244b6` | `include/Config.h`, `src/Config.cpp`, `src/main.cpp`, UI/docs config files | Secret branch grep passed; final firmware build required | Web OTA endpoints remain unauthenticated and should get optional auth later |
| AUDIT-009 | High | Fixed | `fix/audit-telemetry-mqtt-ascom` `6511961` | `src/TCPServer.cpp`, `include/TCPServer.h`, `src/main.cpp` | Worker `pio run` passed | Validate ASCOM `:051#` against real RG-15 hardware |
| AUDIT-010 | High | Fixed | `fix/audit-firmware-async-state` `26c0c92`, UI `0337c3f` | `src/WebServer.cpp`, `include/WebServer.h`, `web/src/components/Dashboard.tsx`, `web/src/mocks/data.ts` | Worker firmware/UI builds passed; final web build required | Field test stale indicators with real sensor cadence |
| AUDIT-011 | High | Fixed | `fix/audit-security-secrets-ota` `f8244b6`, config `b81d313` | `src/Config.cpp`, `src/WebServer.cpp`, UI config/types/docs | Secret grep passed in worker; final secret grep required | Add optional HTTP auth later |
| AUDIT-012 | High | Fixed | `fix/audit-firmware-async-state` `26c0c92` | `src/main.cpp` | Worker `pio run` passed | Document recovery path if LittleFS is missing |
| AUDIT-013 | High | Fixed | `fix/audit-architecture-cleanup` `d11421d` partial | `src/sensors/TSL2591Sensor.cpp` | Worker `pio run` passed | Add lux/SQM unit tests |

## Medium And Low Findings

| ID | Severity | Status | Branch/commit | Notes |
|----|----------|--------|---------------|-------|
| AUDIT-014 | Medium | Fixed | `6511961` | MQTT payload emits epoch seconds when time is valid and `timeValid` otherwise |
| AUDIT-015 | Medium | Fixed | `6511961` | MQTT retained availability and LWT added under `<topic>/availability` |
| AUDIT-016 | Medium | Fixed | `09aaa43` | REST status docs updated |
| AUDIT-017 | Medium | Fixed | `09aaa43` | WiFi scan docs updated |
| AUDIT-018 | Medium | Fixed | `26c0c92` | RG-15 wait loop yields |
| AUDIT-019 | Medium | Fixed | `26c0c92` | `minFreeHeap` and `maxAllocHeap` added |
| AUDIT-020 | Medium | Fixed | `26c0c92` | reset reason and RTC boot count added |
| AUDIT-021 | Medium | Fixed | `0337c3f` | validation errors now use canonical keys/count |
| AUDIT-022 | Medium | Fixed | `0337c3f`, `b81d313` | MSW config save returns `{success:true}` |
| AUDIT-023 | Medium | Fixed | `09aaa43` | changelog GPS status corrected |
| AUDIT-024 | Medium | Fixed | `26c0c92` | cloud payload includes humidity source/BME availability |
| AUDIT-025 | Medium | Partially fixed | `0337c3f` | UI labels metric/imperial; `switch` mode remains limited by current backend contract |
| AUDIT-026 | Medium | Fixed | `7fa2f21` | screenshot CI no longer continues on error; tests now assert content and save screenshots |
| AUDIT-027 | Medium | Fixed | `7fa2f21` | async libraries pinned to commit hashes |
| AUDIT-028 | Medium | Fixed | `7fa2f21` | `package-lock.json` committed; CI uses `npm ci` and lockfile cache key |
| AUDIT-029 | Low | Fixed | `d11421d` partial | logger defaults to INFO unless `DEBUG_BUILD` |
| AUDIT-030 | Low | Fixed | `0337c3f` | WebSocket hook uses backoff and cleanup refs |
| AUDIT-031 | Low | Fixed | `6511961` | TCP server restarts after WiFi reconnect |
| AUDIT-032 | Low | Fixed | `6511961` | GPS HDOP normalized in sensor payload |

## Validation Evidence

Branch-level validation completed before integration:

- `fix/audit-firmware-async-state`: `platformio run` passed; `platformio test` not applicable because no firmware `test/` directory exists.
- `fix/audit-security-secrets-ota`: targeted secret grep and `git diff --check` passed; final integrated builds required.
- `fix/audit-config-api-settings`: `pio run`, `npm run build`, and `git diff --check` passed.
- `fix/audit-telemetry-mqtt-ascom`: `pio run` and `git diff --check` passed.
- `fix/audit-ui-msw-ux`: `npm run build` and `npm run build:demo` passed; old screenshot baseline mode failed before CI branch replaced it.
- `fix/audit-tests-ci-build`: `npm ci`, `npm run typecheck`, `npm run build`, `npm run build:demo`, `npm run screenshots`, `mkdocs build --strict`, `platformio run`, and `platformio run --target buildfs` passed in the worker.
- `fix/audit-docs-remediation`: `mkdocs build --strict` passed during integration using the existing venv MkDocs binary.
- `fix/audit-architecture-cleanup`: `pio run` passed.

Final integrated validation is recorded in the PR body.

## Deferred Work

- Add optional HTTP authentication for config-mutating and web OTA endpoints.
- Add firmware unit tests for config parsing, sky quality, cloud detection, RG-15 parsing, and MQTT payloads.
- Add hardware validation for OTA, WiFi scan/connect, ASCOM rain rate, and MQTT availability.
- Improve RG-15 `switch` unit reporting so the UI can display the physical switch-selected units exactly.
- Keep RG-15 feature expansion out of this remediation PR; further RG-15 feature work should happen after this lands.
