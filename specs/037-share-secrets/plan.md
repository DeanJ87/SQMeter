# Implementation Plan: Share passwords with a paired device

**Branch**: `spec/037-share-secrets` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

## Summary

A new setting, `sharing.secrets` (off by default), lets a paired device such as RoofRoof read four
stored secrets (MQTT password, ntfy token, Pushover user key and application token) from a new
password-protected request, `GET /api/secrets`. The decision (which refusal, if any) and the
answer's JSON are pure functions in `lib/ConfigModel` (`SharedSecrets`), with a small failed-login
lock (`LoginLock`) next to them, so the firmware, the demo core and the native tests share one
implementation. The firmware handler adds the login check, `Cache-Control: no-store` and a log line
without values. The web UI gets one toggle in Settings → Device → Security with a "?" and a warning
note, disabled by a new settings dependency (D-53) while no device password is set.

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 3.x / ESP-IDF 5.5), TypeScript (strict), Python 3 tools

**Primary Dependencies**: ArduinoJson 6, ESPAsyncWebServer (existing `requireAuth`), Preact + Vite,
Unity (native tests), Vitest, Emscripten (demo core)

**Storage**: NVS main config JSON: one new section `"sharing":{"secrets":false}` (~25 bytes)

**Testing**: `pio test -e native` (new `test_shared_secrets`; additions to `test_config_model` and
`test_settings_deps`), Vitest (Security card toggle, D-53), the settings-deps, i18n, API routes,
OpenAPI, quality and docs checks

**Target Platform**: ESP32 / ESP32-S3 firmware, the web UI, the WebAssembly demo core

**Project Type**: embedded firmware + web UI

**Performance Goals**: one small JSON document (< 512 bytes) per request; no new task or buffer

**Constraints**: main config JSON within 5100 bytes (worst-case fit tests); handler on the async
TCP task does no network or long work (Principle IV); gzipped UI growth well under 10 KB (SIZE-01);
both firmware images within their slots (SIZE-04)

**Scale/Scope**: one setting, one route, one dependency, ~5 UI strings in 14 languages

## Constitution Check

*GATE: checked before research and again after design.*

| Principle | How this plan meets it | Result |
|---|---|---|
| I. Fail-safe verdict | Not touched | Pass |
| II. Alpaca conformance | No Alpaca change | Pass |
| III. Testable pure logic | Refusal order, JSON shape and the login lock are pure (`lib/ConfigModel/SharedSecrets`) with native tests; the toggle and D-53 have Vitest tests | Pass |
| IV. Budgets | No task or heap kept; config +25 bytes inside the worst-case tests; handler builds < 512 bytes of JSON | Pass |
| V. Quiet UI | One `DepToggle` in the existing Security card, "?" hint, one-line note, disabled with reason and fix link (D-53) | Pass |
| VI. Trusted-LAN security | The route calls `requireAuth` and refuses without a device password. It returns unmasked secrets, which Principle VI forbids today | Pass with amendment (see Complexity Tracking) |
| VII. Docs | REST, OpenAPI, security, configuration, settings-dependencies reference, CHANGELOG | Pass |
| VIII. Quality | Coding standard followed; `check.py` before the PR | Pass |

## Project Structure

### Documentation (this feature)

```text
specs/037-share-secrets/
├── spec.md
├── plan.md            # this file
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── api.md         # GET /api/secrets and sharing.secrets
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
lib/ConfigModel/include/Config.h            # SharingConfig, Config::sharing
lib/ConfigModel/include/SharedSecrets.h     # refusal, answer JSON, LoginLock (new)
lib/ConfigModel/src/SharedSecrets.cpp       # (new)
lib/ConfigModel/src/ConfigModel.cpp, ConfigJsonIn.cpp, ConfigJsonOut.cpp
lib/SettingsDeps/ (+ catalogue.json)        # D-53, reason no-web-password
src/WebServerSecrets.cpp, include/WebServer.h  # GET /api/secrets (registered in WebServerApi.cpp)
tools/demo-core/                            # the demo answers it; core rebuilt
tools/api/{routes,operations}.json, openapi.py, docs/api/openapi.json
specs/016-demo-device-emulation/contracts/schemas/config.schema.json
web/src/components/settings/DeviceTab.tsx   # the toggle
web/src/{types,validation,lib,mocks,i18n,demo}/ ...
test/test_shared_secrets/                   # new native tests
docs/ , CHANGELOG.md, .specify/memory/constitution.md
```

**Structure Decision**: the pure logic goes in `lib/ConfigModel`, which already owns the secrets
and their masking (STRUCT-02); the firmware keeps only the HTTP glue.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Principle VI: "Secrets MUST be masked in API responses". `GET /api/secrets` returns four secrets unmasked. | A paired device can't copy the MQTT password, ntfy token or Pushover keys any other way; the owner retypes them by hand on every change. | Unmasking them in `GET /api/config` would expose them to every LAN client without a login. An encrypted backup file needs a passphrase typed on both devices and carries every secret, including WiFi. |

The amendment (MINOR, 1.4.0 → 1.5.0) keeps masking everywhere else and adds the one exception: an
owner opt-in, off by default, that needs the device password set and a correct login, and returns
only those four values over a route that is never cached or logged.

Post-design re-check: the design adds no further violations.
