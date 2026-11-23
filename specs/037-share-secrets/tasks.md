# Tasks: Share passwords with a paired device

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/api.md](contracts/api.md)

**Tests**: native tests are required by Principle III and SC-002/SC-003/SC-005; Vitest for the
toggle and D-53.

## Phase 1: Setup

- [X] T001 Amend Principle VI (MINOR, 1.4.0 → 1.5.0) with the one opt-in, password-protected exception to masking in .specify/memory/constitution.md

## Phase 2: Foundational (blocks every story)

- [X] T002 `SharingConfig` and `Config::sharing` (default off) in lib/ConfigModel/include/Config.h, src/ConfigModel.cpp; JSON in/out (`sharing.secrets`) in src/ConfigJsonIn.cpp and src/ConfigJsonOut.cpp
- [X] T003 [P] Native config tests: default off, older config without `sharing` loads off, round trip, not masked, wrong type rejected; worst-case fit tests still pass, in test/test_config_model/

## Phase 3: US1 + US2 - the request (P1)

**Goal**: a paired device with the right login gets the four values; everything else is refused.
**Independent test**: `pio test -e native -f test_shared_secrets`.

- [X] T004 [US1] Native tests for refusal order, the answer's exact shape and keys, `""` for unset values, no other secret in it, and `LoginLock` (five failures, 60 s, reset on success, wrap-around) in test/test_shared_secrets/test_main.cpp
- [X] T005 [US1] `SharedSecrets::refusal()`, `refusalMessage()`, `devicePasswordSet()`, `toJson()` and `LoginLock` in lib/ConfigModel/include/SharedSecrets.h and lib/ConfigModel/src/SharedSecrets.cpp
- [X] T006 [US2] `GET /api/secrets` in src/WebServerSecrets.cpp (registered from src/WebServerApi.cpp) and include/WebServer.h: refusal → 403, lock → 429 with `Retry-After`, `requireAuth` → 401, 200; `Cache-Control: no-store`; log without values
- [X] T007 [US1] Route registry and OpenAPI: tools/api/routes.json, tools/api/operations.json (403 and 429 statuses in tools/api/openapi.py), `sharing` in specs/016-demo-device-emulation/contracts/schemas/config.schema.json; regenerate docs/api/openapi.json
- [X] T008 [US1] The demo answers it: `EmulatedDevice::secretsRequest()` in tools/demo-core, bridge, web/src/demo/device.ts and handlers.ts; rebuild the core (tools/demo-core/build.sh)

## Phase 4: US3 - the owner can turn it on and understand it (P2)

- [X] T009 [US3] D-53 (`sharing.secrets` needs a device password, reason `no-web-password`, fix `device#security`) in lib/SettingsDeps/catalogue.json, lib/SettingsDeps/src/SettingsDeps.cpp, web/src/lib/settingsDeps.ts, web/src/lib/settingsDepReasons.ts; fixture cases in test/fixtures/settings-deps/cases.json; default-config fixture; native and web tests name D-53; inventory entry in web/src/dashboard/inventory.json
- [X] T010 [US3] Web types, Zod schema, defaults and mocks for `sharing.secrets` in web/src/types/config.ts, web/src/validation/configSchema.ts, web/src/components/settings/defaults.ts, web/src/mocks/config.ts
- [X] T011 [US3] Share passwords `DepToggle`, "?" hint and on-note in the Security card, web/src/components/settings/DeviceTab.tsx; label in depLabels.ts
- [X] T012 [US3] Strings in web/src/i18n/en.json and en.context.json, translated into all 13 locales, recorded in tools/i18n/record.json
- [X] T013 [US3] Vitest: off by default, disabled without a device password (D-53), enabled with one, note shown when on, saved in the payload, in web/src/components/__tests__/SharePasswords.test.tsx

## Phase 5: Polish

- [X] T014 Docs: docs/api/rest.md (`GET /api/secrets`), docs/user-guide/security.md (what is shared, the LAN risk, plain HTTP, the lock), docs/user-guide/configuration.md (`sharing.secrets`), docs/reference/settings-dependencies.md (regenerated), CHANGELOG.md
- [X] T015 Run the gates: native tests, web typecheck/lint/Vitest, firmware builds (esp32dev, esp32dev-ble) and size baseline, settings-deps, i18n, inventory, API routes/OpenAPI, demo core hash, UI size, `tools/quality/check.py`, `mkdocs build --strict`

## Dependencies

T001 first; T002 before T004-T011; T005 before T006 and T008; T009 before T011; T012 before T013.
