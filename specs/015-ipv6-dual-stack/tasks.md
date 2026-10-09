# Tasks: IPv6 (Dual Stack)

**Input**: [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md),
[quickstart.md](quickstart.md)

## Phase 1: Setup

- [ ] T001 Pin `esp32async/AsyncTCP@3.4.10` in platformio.ini (dual-stack listening, research R2)
- [ ] T002 [P] Create `lib/NetAddress/include/NetAddress.h`, `lib/NetAddress/src/NetAddress.cpp` and `test/fixtures/net-address/cases.json`

## Phase 2: Foundational

- [ ] T003 Implement IPv6 parse (RFC 4291 text incl. `::` and embedded IPv4), RFC 5952 format, scope classification and `allowedPeer` in lib/NetAddress/src/NetAddress.cpp
- [ ] T004 Implement `parseHost` (broker forms, data-model table) and `parseHttpUrl` (bracket-aware, port, path; https + IPv6 literal rejected) in lib/NetAddress/src/NetAddress.cpp
- [ ] T005 [P] Native tests driven by test/fixtures/net-address/cases.json in test/test_net_address/test_main.cpp
- [ ] T006 [P] Browser mirror web/src/lib/netAddress.ts and web/src/lib/__tests__/netAddress.test.ts on the same fixtures

## Phase 3: US1 + US2 - reach the device over IPv6, nothing breaks on IPv4 (P1)

- [ ] T007 [US2] `wifi.ipv6` (default true; missing → true) in lib/ConfigModel/include/Config.h and ConfigModel.cpp (defaults, toJson, applyJson) with tests in test/test_config_model
- [ ] T008 [US1] Enable IPv6 on `ARDUINO_EVENT_WIFI_STA_CONNECTED` when `wifi.ipv6` is on; collect addresses with `esp_netif_get_all_ip6` in src/WiFiManager.cpp / include/WiFiManager.h
- [ ] T009 [US1] LAN-only middleware for IPv6 peers (403 otherwise) using `NetAddress::allowedPeer` in src/WebServer.cpp
- [ ] T010 [US1] Confirm mDNS answers AAAA once IPv6 is up (no code if R4 holds on device); log it in quickstart results

## Phase 4: US5 - see and control IPv6 (P2)

- [ ] T011 [US5] `/api/status` `wifi.ipv6 = {enabled, addresses[{address, scope}]}` in src/WebServer.cpp; types in web/src/types/index.ts
- [ ] T012 [US5] System page lists IPv6 addresses with scope (web/src/components/System.tsx) with a unit test
- [ ] T013 [US5] Settings → Network: IPv6 toggle beside mDNS, restart-to-apply (web/src/components/settings/NetworkTab.tsx, Settings.tsx restart reasons, configSchema.ts) with a unit test
- [ ] T014 [US5] Demo shows IPv6 addresses and honours the toggle (web/src/demo/handlers.ts, web/src/mocks/data.ts)
- [ ] T015 [US5] Contract schemas/samples include `wifi.ipv6` (specs/016-demo-device-emulation/contracts, tools/contracts) and contract-check passes

## Phase 5: US3 - outbound over IPv6 (P2)

- [ ] T016 [US3] `DualStackClient` (getaddrinfo AF_UNSPEC, IPv6 literal with/without brackets, AF_INET6 connect with timeout) in include/DualStackClient.h, src/DualStackClient.cpp
- [ ] T017 [US3] MQTT client and the MQTT test use DualStackClient and `parseHost` (bracketed broker, embedded port) in src/MQTTClient.cpp, include/MQTTClient.h, src/WebServer.cpp
- [ ] T018 [US3] http webhooks: parse URL with `parseHttpUrl`, use DualStackClient, `HTTPClient::begin(client, host, port, uri, https)` with bracketed host in src/AlertDispatcher.cpp
- [ ] T019 [US3] Validation for broker and webhook URL on API changes in lib/ConfigModel/src/ConfigModel.cpp and in web/src/validation/configSchema.ts, with tests on both sides

## Phase 6: US4 - Alpaca discovery over IPv6 (P3)

- [ ] T020 [US4] Second AsyncUDP on `ff12::a1:2345`:32227 started once an IPv6 address exists (Alpaca and IPv6 on) in src/WebServer.cpp / include/WebServer.h

## Phase 7: Polish

- [ ] T021 Amend spec.md FR-006/FR-008 scope and assumptions per research R2/R6
- [ ] T022 Docs: IPv6 in docs/user-guide/configuration.md (network), REST status reference, troubleshooting; diagram check
- [ ] T023 Gates: native tests, both firmware builds (sizes), web tsc/vitest/build/build:demo, Playwright, quality check, settings-deps, diagrams, SOURCE_HASH, mkdocs --strict
- [ ] T024 Device verification on the spare per quickstart.md (record results in quickstart.md)
