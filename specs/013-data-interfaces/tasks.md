---
description: "As-built task list (backfill) for Data Interfaces"
---

# Tasks: Data Interfaces (MQTT, REST, WebSocket)

**Input**: Design documents from `specs/013-data-interfaces/`

**Prerequisites**: plan.md, spec.md

**Note**: Backfill of `main` @ `b1d382e`; existing work is marked done.

## Phase 1: Setup

- [X] T001 PubSubClient connection, availability (LWT) and reconnect in src/MQTTClient.cpp

## Phase 2: Foundational

- [X] T002 MQTT config (broker, credentials, topic, interval) in src/Config.cpp and web/src/components/settings/NetworkTab.tsx

## Phase 3: User Story 1 - One data model everywhere (P1)

- [X] T003 [US1] REST/WebSocket readings in src/WebServer.cpp (createSensorJson)
- [X] T004 [US1] MQTT readings in src/MQTTClient.cpp (createPayload)

## Phase 4: User Story 2 - MQTT (P1)

- [X] T005 [US2] safety, safe, alerts, alerts/armed topics and alerts/armed/set command in src/WebServer.cpp, src/AlertDispatcher.cpp, src/MQTTClient.cpp

## Phase 5: User Story 3 - REST API (P2)

- [X] T006 [US3] REST endpoints and error helper in src/WebServer.cpp

## Phase 6: User Story 4 - Live streams (P3)

- [X] T007 [US4] /ws/sensors (1 s) and /ws/status (2 s) in src/WebServer.cpp

## Phase 7: Polish & Cross-Cutting

- [X] T008 [P] docs/user-guide/mqtt.md, docs/api/rest.md, docs/api/websocket.md

## Phase 8: Convergence

- [ ] T009 CRITICAL: Stop publishing light, sky and environment values over MQTT when their sensor is faulted (currently sent unconditionally, so a dead BME280 publishes zeros as readings) in src/MQTTClient.cpp (createPayload) per FR-006 / Constitution I (contradicts)
- [ ] T010 Generate REST, WebSocket and MQTT readings from one shared serializer so group and field names agree — today MQTT uses light/sky/infrared/clouds/location/rain vs REST lightSensor/skyQuality/irTemperature/cloudConditions/gps/rainSensor, skyTemp vs objectTemp, coverPercent vs cloudCoverPercent, gain vs gainName — in src/MQTTClient.cpp and src/WebServer.cpp per FR-001 / SC-001 (contradicts)
- [ ] T011 Add the missing MQTT values: environment dew point, wind (speed, gust, direction, validity) and per-sensor validity/age, in src/MQTTClient.cpp per FR-005 (missing)
- [ ] T012 Remove RG-15 UART diagnostics (≈60 fields: counters, raw responses, timings) from the default MQTT readings and make them an opt-in diagnostics group, in src/MQTTClient.cpp per FR-005 / SC-004 (unrequested)
- [ ] T013 Omit the MQTT rain group (and /api/status sensors.rg15) when the rain sensor is disabled, in src/MQTTClient.cpp and src/WebServer.cpp (createStatusJson) per FR-006 / FR-010 (contradicts)
- [ ] T014 Use camelCase for every key and drop aliases (rain: rain_intensity/rInt, total_accumulation/totalAcc, event_accumulation/local_event_accumulation/hydreon_event_accumulation/eventAcc, accumulation_since_last_read/acc; uart.*; gps age vs ageMs) in src/WebServer.cpp, src/MQTTClient.cpp and the web UI types per FR-002 / SC-002 (contradicts)
- [ ] T015 Emit timestamps as Unix seconds only, with timeValid as the flag — MQTT `timestamp` currently switches to milliseconds since boot without a clock — in src/MQTTClient.cpp per FR-003 (contradicts)
- [ ] T016 Restructure MQTT topics into one hierarchy with readings as a sibling (e.g. <base>/state, <base>/availability, <base>/safe, <base>/safety, <base>/alerts, <base>/alerts/armed[/set]) instead of children of the readings topic, and use one boolean convention (online/offline vs 1/0), in src/MQTTClient.cpp, src/WebServer.cpp and src/AlertDispatcher.cpp per FR-004 (contradicts)
- [ ] T017 Add MQTT publish settings — per-group on/off (each sensor, safety, alerts, diagnostics) — to the mqtt config in src/Config.cpp and web/src/components/settings/NetworkTab.tsx per FR-007 / US2-AC3 (missing)
- [ ] T018 Add opt-in Home Assistant MQTT discovery for readings, safe flag and the alerts switch in src/MQTTClient.cpp per FR-008 / US2-AC5 (missing)
- [ ] T019 Return non-2xx status codes with {"error": "..."} for failed firmware/filesystem uploads (now 200 with {"success": false}) and the RG-15 actions, and use one success shape ({"success": true} vs {"ok": ...} vs {"armed": ...}) in src/WebServer.cpp per FR-009 / SC-005 (contradicts)
- [ ] T020 Add payload-schema tests (field names, presence rules, units) for the shared serializer per Constitution III (missing)
- [ ] T021 Rewrite docs/user-guide/mqtt.md payload and topic tables to match the device exactly (missing safe, timeValid, rain units; disabled-sensor rules) and update docs/api/rest.md / docs/api/websocket.md for the unified schema, per FR-011 / SC-003 (partial)
