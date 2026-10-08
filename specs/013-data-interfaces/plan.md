# Implementation Plan: Data Interfaces (MQTT, REST, WebSocket)

**Branch**: n/a (backfill of `main` @ `b1d382e`) | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/013-data-interfaces/spec.md`

**Note**: As-built plan for `/speckit-converge`.

## Summary

Readings are serialised twice, independently: `WebServer::createSensorJson` (REST `/api/sensors`
and `/ws/sensors`) and `MQTTClient::createPayload` (MQTT `<topic>`). Status is
`WebServer::createStatusJson` (`/api/status`, `/ws/status`). MQTT also carries
`<topic>/availability` (`online`/`offline`), `<topic>/safety` (JSON), `<topic>/safe` (`1`/`0`),
`<topic>/alerts` (JSON) and `<topic>/alerts/armed` (`1`/`0`), and subscribes to
`<topic>/alerts/armed/set`. REST handlers build their own response bodies.

## Technical Context

**Language/Version**: C++17; TypeScript (Preact)
**Primary Dependencies**: PubSubClient (3072-byte buffer), ESPAsyncWebServer, ArduinoJson 6
**Storage**: NVS (`mqtt`: enabled, broker, port, username, password, topic, publishIntervalMs)
**Testing**: Vitest (UI); no payload-schema tests
**Target Platform**: ESP32
**Project Type**: Embedded firmware
**Constraints**: MQTT buffer 3072 bytes; heap

## Constitution Check

| Principle | Gate | As-built |
|---|---|---|
| I. Fail-safe | Faulted sensors not reported as valid values | MQTT light/sky/environment sent regardless of status |
| III. Testable pure logic | Payload schema testable off-device | Serialisers live in firmware glue; untested |
| IV. Budgets | Payload within MQTT buffer | Readings incl. RG-15 diagnostics near 3 KB |
| VI. Security | MQTT password masked | Yes |
| VII. Docs | Docs match field-for-field | docs/user-guide/mqtt.md, docs/api/rest.md, docs/api/websocket.md |

## Project Structure

### Documentation (this feature)

```text
specs/013-data-interfaces/  spec.md, plan.md, tasks.md, checklists/requirements.md
```

### Source Code (repository root)

```text
src/MQTTClient.cpp, include/MQTTClient.h   # createPayload, availability, publishSubtopic, commands
src/WebServer.cpp                          # createSensorJson, createStatusJson, appendRG15Diagnostics,
                                           # publishMqttSafety, publishArmedState, REST handlers, createErrorJson
src/AlertDispatcher.cpp                    # <topic>/alerts
src/Config.cpp, include/Config.h           # MQTTConfig
web/src/components/settings/NetworkTab.tsx # MQTT settings
docs/user-guide/mqtt.md, docs/api/rest.md, docs/api/websocket.md, docs/user-guide/security.md
```

**Structure Decision**: Single firmware project.

## Complexity Tracking

None.
