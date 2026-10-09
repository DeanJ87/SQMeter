# Implementation Plan: IPv6 (Dual Stack)

**Branch**: `feat/015-ipv6` | **Date**: 2026-10-09 | **Spec**: [spec.md](spec.md)

## Summary

Turn on IPv6 on the joined WiFi network (link-local + SLAAC), make the web server listen on both
families by moving to AsyncTCP 3.4.10, restrict IPv6 peers to the local network with a
middleware, answer Alpaca discovery on `ff12::a1:2345`, let MQTT and http webhooks reach IPv6
literals and IPv6-only names through a dual-stack client, accept IPv6 literals in settings with
the same rules on device and browser, show the addresses in status, and add an IPv6 switch.
TLS and NTP over IPv6 are out of reach on Arduino-ESP32 2.0.17 and move to a platform-upgrade
spec (research R6; spec amended).

## Technical Context

**Language/Version**: C++17 (Arduino-ESP32 2.0.17, ESP-IDF 4.4), TypeScript (Preact + Vite)
**Primary Dependencies**: ESPAsyncWebServer 3.6.0, AsyncTCP 3.4.10 (was 3.3.2), lwIP, ESP mdns
**Storage**: one new boolean in the main config JSON (`wifi.ipv6`)
**Testing**: PlatformIO native (Unity) for `lib/NetAddress` and config; Vitest for the web
mirror and UI; Playwright for the demo; on-device checks on the spare device
**Target Platform**: ESP32 (standard and Bluetooth builds)
**Constraints**: both images fit their slots; main-loop stack headroom > 2 KB; no IPv4
regression (FR-013)

## Constitution Check

| Principle | Check |
|---|---|
| I. Fail-safe verdict | No change to safety logic. |
| II. Alpaca conformance | Discovery reply identical over IPv6; ConformU on device. |
| III. Testable pure logic | Address parsing, classification, peer policy, URL/host validation in `lib/NetAddress` with native tests; shared fixtures with the web mirror. |
| IV. Resource budgets | Library swap measured (-300 B); heap/stack measured on device with IPv6 on. |
| V. Quiet UI | One toggle beside mDNS, addresses on the System page. |
| VI. Trusted LAN | FR-011 middleware: IPv6 only from link-local / on-link /64. Docs say it doesn't make the device internet-safe. |
| VII. Docs | Network section of the configuration reference, REST status reference, troubleshooting. |
| VIII. Code quality | `tools/quality/check.py` with no new findings. |
| Platform constraints | Library pin change only; Arduino core unchanged. No platform upgrade (deferred, R6). |

## Project Structure

```text
lib/NetAddress/{include/NetAddress.h, src/NetAddress.cpp}   # parse/format/classify, peer policy, host & URL parsing
test/test_net_address/test_main.cpp                          # native tests + shared fixtures
test/fixtures/net-address/cases.json                         # shared with web/src/lib/netAddress.test.ts
lib/ConfigModel                                              # wifi.ipv6; broker/webhook rules (API changes only)
include/DualStackClient.h, src/DualStackClient.cpp           # WiFiClient over getaddrinfo, AF_INET6 sockets
src/WiFiManager.cpp                                          # enableIpV6 on STA connect, addresses
src/WebServer.cpp                                            # LAN-only middleware, IPv6 discovery, status, MQTT test client
src/MQTTClient.cpp, src/AlertDispatcher.cpp                  # DualStackClient, bracket-aware webhook URLs
web/src/lib/netAddress.ts (+ test)                           # browser mirror
web/src/validation/configSchema.ts, settings Network tab, System page, types, demo status
docs/: configuration, REST status, troubleshooting
```

## Phases

1. Setup: library pin, `lib/NetAddress` skeleton, fixtures.
2. Foundational: NetAddress logic + native tests; web mirror + tests on the same fixtures.
3. US1/US2 (P1): IPv6 on connect, dual-stack listening, LAN-only middleware, mDNS behaviour,
   setting default on, off switch → IPv4-only.
4. US5 (P2): status addresses, System page, Settings toggle, demo.
5. US3 (P2): DualStackClient for MQTT/MQTT test/http webhooks; validation on both sides.
6. US4 (P3): IPv6 Alpaca discovery.
7. Polish: docs, spec amendment (FR-006 scope), contracts, quality/diagrams/deps checks,
   device verification.
