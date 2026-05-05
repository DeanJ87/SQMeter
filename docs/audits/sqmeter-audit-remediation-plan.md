# SQMeter Audit Remediation Plan

This matrix tracks the 32 findings from `docs/audits/sqmeter-nuclear-codebase-audit.md`.

Statuses:

| Status | Meaning |
|--------|---------|
| Docs closed | Documentation was corrected in this branch for the verified current behavior |
| Partial | Documentation now warns about the limitation, but firmware/UI/build work remains |
| Open | No local remediation in this docs branch |
| Deferred | Out of this worker's scope or needs another worker's implementation |

## Closure Matrix

| ID | Severity | Area | Status | Local evidence | Validation evidence |
|----|----------|------|--------|----------------|---------------------|
| AUDIT-001 | Critical | Async callbacks | Partial | REST/OTA docs warn that WiFi scan and OTA handlers are blocking/unauthenticated where applicable | Integration placeholder: firmware async refactor and WDT regression test |
| AUDIT-002 | Critical | Sensor concurrency | Open | No docs can close firmware data-race risk | Integration placeholder: firmware mutex/double-buffer implementation and stress test |
| AUDIT-003 | Critical | Credential logging | Partial | Configuration and README now document current trusted-LAN/credential exposure model | Integration placeholder: firmware removes plaintext config logging |
| AUDIT-004 | High | Config method mismatch | Partial | REST docs explicitly state `POST /api/config`, not `PUT`; firmware still needs UI contract verification | Integration placeholder: Settings save browser/API regression test |
| AUDIT-005 | High | MQTT client ID | Partial | MQTT guide warns current firmware uses publish topic as MQTT client ID | Integration placeholder: firmware MAC/client-id fix and broker two-device test |
| AUDIT-006 | High | WiFi scan blocking | Docs closed | `docs/api/rest.md` now shows actual `/api/wifi/scan` payload and warns scan is synchronous | Docs build/link check |
| AUDIT-007 | High | NVS JSON size | Partial | Config docs document full current schema and trusted config handling | Integration placeholder: firmware compact JSON and NVS size guard |
| AUDIT-008 | High | ArduinoOTA auth | Partial | README/config/OTA docs warn OTA and ArduinoOTA are unauthenticated LAN capabilities | Integration placeholder: firmware OTA password/auth mitigation |
| AUDIT-009 | High | ASCOM rain rate | Partial | `docs/api/ascom.md` documents `:051#` currently returns `0.0` and is unsafe for rain interlocks | Integration placeholder: firmware wires RG-15 into TCPServer and ASCOM client test |
| AUDIT-010 | High | WebSocket stale data | Partial | WebSocket docs warn 1s broadcast can repeat 5s sensor data and lacks timestamp | Integration placeholder: firmware data timestamp or broadcast cadence fix |
| AUDIT-011 | High | `/api/config` passwords | Partial | REST/config docs warn `GET /api/config` returns password fields without auth | Integration placeholder: firmware masks passwords and preserves existing secrets on empty save |
| AUDIT-012 | High | LittleFS format-on-fail | Open | No docs claim filesystem mount failure is recoverable | Integration placeholder: firmware mount failure handling |
| AUDIT-013 | High | TSL2591 CPL formula | Open | Out of docs scope; no user-facing formula guarantee changed | Integration placeholder: firmware constant cleanup and calculation unit tests |
| AUDIT-014 | Medium | MQTT timestamp | Partial | MQTT guide documents current `timestamp` as uptime milliseconds, not epoch | Integration placeholder: TimeManager-backed epoch timestamp and consumer test |
| AUDIT-015 | Medium | MQTT LWT | Partial | MQTT guide warns there is no availability/LWT topic | Integration placeholder: broker availability topic test |
| AUDIT-016 | Medium | `/api/status` docs | Docs closed | `docs/api/rest.md` now documents actual nested status shape from `src/WebServer.cpp` | Docs build/link check |
| AUDIT-017 | Medium | WiFi scan docs | Docs closed | `docs/api/rest.md` now documents `{networks:[...]}` and `encryption` field | Docs build/link check |
| AUDIT-018 | Medium | RG-15 busy wait | Open | RG-15 hardware docs were already present; firmware polling risk remains | Integration placeholder: firmware yield/state-machine fix and rain polling soak test |
| AUDIT-019 | Medium | Heap diagnostics | Partial | REST docs include current status fields and warn `minFreeHeap` is absent | Integration placeholder: firmware adds `minFreeHeap`/`maxAllocHeap` |
| AUDIT-020 | Medium | Reset reason/boot count | Partial | REST docs warn reset reason and boot count are absent | Integration placeholder: firmware adds reset diagnostics |
| AUDIT-021 | Medium | Settings validation count | Deferred | UI implementation is outside this docs worker's ownership | Integration placeholder: UI unit test for validation count |
| AUDIT-022 | Medium | Mock config response | Deferred | UI mock implementation is outside this docs worker's ownership | Integration placeholder: mock/API contract test |
| AUDIT-023 | Medium | GPS changelog | Docs closed | `CHANGELOG.md` moved GPS time source implementation into `0.0.1` | Changelog diff review |
| AUDIT-024 | Medium | Cloud humidity fallback | Partial | REST docs expose current `humidityUsed`; no firmware fallback-source field yet | Integration placeholder: firmware `humiditySource` field and BME failure test |
| AUDIT-025 | Medium | Rain units UI | Partial | WebSocket/API/MQTT docs clarify current rain payload fields do not carry unit metadata | Integration placeholder: UI displays configured rain units |
| AUDIT-026 | Medium | Screenshot CI ignored | Deferred | CI workflow outside this docs branch scope unless assigned to CI worker | Integration placeholder: docs workflow removes `continue-on-error` after screenshot stability |
| AUDIT-027 | Medium | Async library pinning | Deferred | Build dependency pinning is outside docs worker scope | Integration placeholder: PlatformIO dependency pin and reproducible build |
| AUDIT-028 | Medium | npm cache key | Deferred | CI workflow outside docs worker scope unless assigned to CI worker | Integration placeholder: workflow cache key changes to `package-lock.json` |
| AUDIT-029 | Low | Logger default DEBUG | Partial | README/config docs warn current trusted-LAN/serial exposure risk; runtime log level remains firmware work | Integration placeholder: firmware defaults INFO or config-driven log level |
| AUDIT-030 | Low | WS reconnect backoff | Deferred | UI hook implementation outside docs worker scope | Integration placeholder: UI reconnect backoff test |
| AUDIT-031 | Low | TCP server reconnect | Partial | ASCOM docs warn integrations must validate runtime behavior and keep TCP on LAN | Integration placeholder: firmware restarts TCP server after WiFi reconnect |
| AUDIT-032 | Low | GPS HDOP inconsistency | Partial | REST docs show current `gps` and `gpsData` behavior where relevant; firmware normalization remains | Integration placeholder: API contract test normalizes HDOP |

## Documentation Remediation Completed

| Finding | Files |
|---------|-------|
| AUDIT-016 | `docs/api/rest.md` |
| AUDIT-017 | `docs/api/rest.md` |
| AUDIT-023 | `CHANGELOG.md` |
| WebSocket stale/rain docs | `docs/api/websocket.md` |
| MQTT stale payload docs | `docs/user-guide/mqtt.md` |
| OTA/update/fs docs | `docs/api/rest.md`, `docs/user-guide/ota.md` |
| ASCOM/TCP docs | `docs/api/ascom.md`, `mkdocs.yml` |
| Config schema/security docs | `docs/user-guide/configuration.md`, `README.md`, `BUILD.md` |

## Integration Evidence Slots

Use this section during final audit closure once firmware/UI/CI workers merge their changes.

| Evidence item | Command or method | Result |
|---------------|-------------------|--------|
| Docs build | `mkdocs build --strict` | Blocked locally: `mkdocs` command not installed |
| Static nav/link check | Python check for nav coverage and relative `.md` links | Passed locally |
| REST status contract | `curl http://<device>/api/status` | Pending hardware/API validation |
| WiFi scan contract | `curl http://<device>/api/wifi/scan` | Pending hardware/API validation |
| WebSocket sensor contract | Connect to `ws://<device>/ws/sensors` | Pending hardware/API validation |
| MQTT payload | `mosquitto_sub -h <broker> -t "sqm/#" -v` | Pending broker validation |
| ASCOM rain-rate command | `printf ':051#' | nc <device> 2020` | Pending firmware fix and hardware validation |
| OTA firmware endpoint | Browser System page or `POST /api/update` | Pending hardware validation |
| OTA filesystem endpoint | Browser System page or `POST /api/update/fs` | Pending hardware validation |

## Risks

- This branch documents verified current behavior; it does not fix firmware safety issues.
- Runtime behavior may change when firmware/UI workers merge their remediation branches. Re-check REST, WebSocket, MQTT, and ASCOM examples before final release docs are published.
- The copied audit report is intentionally included under `docs/audits/` for traceability.
