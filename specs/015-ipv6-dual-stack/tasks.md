# Tasks: IPv6 (Dual Stack)

**Input**: [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md),
[quickstart.md](quickstart.md)

## Phase 1: Setup

- [x] T001 Decide how the web server serves IPv6 on the pinned AsyncTCP (research R2: 3.4.10 tried on the device and rejected - it breaks HTTP firmware uploads; a second IPv6-only listener instead)
- [x] T002 [P] Create `lib/NetAddress/include/NetAddress.h`, `lib/NetAddress/src/NetAddress.cpp` and `test/fixtures/net-address/cases.json`

## Phase 2: Foundational

- [x] T003 Implement IPv6 parse (RFC 4291 text incl. `::` and embedded IPv4), RFC 5952 format, scope classification and `allowedPeer` in lib/NetAddress/src/NetAddress.cpp
- [x] T004 Implement `parseHost` (broker forms, data-model table) and `parseHttpUrl` (bracket-aware, port, path; https + IPv6 literal rejected) in lib/NetAddress/src/NetAddress.cpp
- [x] T005 [P] Native tests driven by test/fixtures/net-address/cases.json in test/test_net_address/test_main.cpp
- [x] T006 [P] Browser mirror web/src/lib/netAddress.ts and web/src/lib/__tests__/netAddress.test.ts on the same fixtures

## Phase 3: US1 + US2 - reach the device over IPv6, nothing breaks on IPv4 (P1)

- [x] T007 [US2] `wifi.ipv6` (default true; missing → true) in lib/ConfigModel/include/Config.h and ConfigModel.cpp (defaults, toJson, applyJson) with tests in test/test_config_model
- [x] T008 [US1] Enable IPv6 on `ARDUINO_EVENT_WIFI_STA_GOT_IP` when `wifi.ipv6` is on (STA_CONNECTED is too early - seen on the device); collect addresses with `esp_netif_get_all_ip6` in src/WiFiManager.cpp / include/WiFiManager.h
- [x] T009 [US1] IPv6-only listener on port 80 feeding the same AsyncWebServer, refusing peers outside the LAN at accept (403, closed before any request) with `NetAddress::allowedPeer` in src/Ipv6Network.cpp
- [ ] T010 [US1] Confirm mDNS answers AAAA once IPv6 is up (no code if R4 holds on device) - needs an mDNS client on the device's subnet (quickstart results)

## Phase 4: US5 - see and control IPv6 (P2)

- [x] T011 [US5] `/api/status` `wifi.ipv6 = {enabled, addresses[{address, scope}]}` in src/WebServer.cpp; types in web/src/types/index.ts
- [x] T012 [US5] System page lists IPv6 addresses with scope (web/src/components/System.tsx) with a unit test
- [x] T013 [US5] Settings → Network: IPv6 toggle beside mDNS, restart-to-apply (web/src/components/settings/NetworkTab.tsx, Settings.tsx restart reasons, configSchema.ts) with a unit test
- [x] T014 [US5] Demo shows IPv6 addresses and honours the toggle (web/src/demo/handlers.ts, web/src/mocks/data.ts)
- [x] T015 [US5] Contract schemas/samples include `wifi.ipv6` (specs/016-demo-device-emulation/contracts, tools/contracts) and contract-check passes

## Phase 5: US3 - outbound over IPv6 (P2)

- [x] T016 [US3] `DualStackClient` (getaddrinfo AF_UNSPEC, IPv6 literal with/without brackets, AF_INET6 connect with timeout) in include/DualStackClient.h, src/DualStackClient.cpp
- [x] T017 [US3] MQTT client and the MQTT test use DualStackClient and `parseHost` (bracketed broker, embedded port) in src/MQTTClient.cpp, include/MQTTClient.h, src/WebServer.cpp
- [x] T018 [US3] http webhooks: parse URL with `parseHttpUrl`, use DualStackClient, `HTTPClient::begin(client, host, port, uri, https)` with bracketed host in src/AlertDispatcher.cpp
- [x] T019 [US3] Validation for broker and webhook URL on API changes in lib/ConfigModel/src/ConfigModel.cpp and in web/src/validation/configSchema.ts, with tests on both sides

## Phase 6: US4 - Alpaca discovery over IPv6 (P3)

- [x] T020 [US4] Discovery socket on `IP_ANY_TYPE`:32227 with IPv6 on, and the `ff12::a1:2345` group joined once an IPv6 address exists (Alpaca and IPv6 on) in src/WebServer.cpp, src/Ipv6Network.cpp

## Phase 7: Polish

- [x] T021 Amend spec.md FR-006/FR-008 scope and assumptions per research R2/R6
- [x] T022 Docs: new docs/user-guide/ipv6.md (with diagram DIA-16, blocking), configuration, REST status, security, Alpaca, alerts, troubleshooting; diagram check
- [x] T023 Gates: native tests, both firmware builds (sizes), web tsc/vitest/build/build:demo, Playwright, quality check, settings-deps, diagrams, SOURCE_HASH, mkdocs --strict
- [x] T024 Device verification on the spare per quickstart.md (results in quickstart.md; the IPv6-client checks need a client on the device's subnet - listed there)

## Phase 8: Convergence (2026-10-09)

- [x] T025 ntfy server URL checked with the same IPv6-aware URL rules as the webhook (FR-007) in lib/ConfigModel/src/ConfigModel.cpp and web/src/validation/configSchema.ts
- [x] T026 NTP servers refuse IPv6 literals with the reason (NTP is IPv4-only here, FR-006 amendment) - lib/NetAddress `isIpv6Literal`, ConfigModel, configSchema, tests
- [x] T027 Alpaca discovery answers IPv6 requests only from the local network (FR-011) - `Ipv6Network::allowedDiscoveryPeer` in src/Ipv6Network.cpp, src/WebServer.cpp

