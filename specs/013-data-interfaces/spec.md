# Feature Specification: Data Interfaces (MQTT, REST, WebSocket)

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

## Clarifications

### Session 2026-10-08

- Q: Renaming fields and topics breaks existing integrations — clean break or deprecated aliases? → A: Clean break. The project is pre-1.0 beta; breaking changes are fine and no aliases are kept.
- Q: Should Home Assistant MQTT discovery be part of this? → A: Yes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - One data model everywhere (Priority: P1)

An integrator reads the same measurement from MQTT, the REST API and the WebSocket and finds it
under the same name, in the same units, with the same meaning.

**Why this priority**: Every integration (Home Assistant, Grafana, scripts) depends on it; drift
between interfaces is the main source of confusion.

**Independent Test**: Compare a REST `/api/sensors` response, a `/ws/sensors` message and an MQTT
readings message taken at the same time: every shared value has one name, one unit, one place.

**Acceptance Scenarios**:

1. **Given** any measurement, **When** it appears on more than one interface, **Then** its group
   name, field name and unit are identical.
2. **Given** any payload, **When** inspected, **Then** keys follow one naming convention
   (camelCase) and no value is sent under two names.
3. **Given** a timestamp, **When** emitted, **Then** it is always Unix seconds, with a separate
   flag when the clock isn't set — never milliseconds since boot in the same field.

---

### User Story 2 - MQTT that makes sense and that I control (Priority: P1)

A Home Assistant user subscribes to a clean topic tree, gets every measurement that matters
(including dew point, wind and sensor validity), doesn't get bring-up diagnostics they never asked
for, and chooses what is published.

**Why this priority**: MQTT is the main route into home automation and logging.

**Independent Test**: Subscribe to `<base>/#` with default settings and check every topic and key
against the documentation; turn groups off in settings and see them disappear.

**Acceptance Scenarios**:

1. **Given** the base topic, **When** the device publishes, **Then** topics follow one documented
   hierarchy (readings, availability, safety, alerts, alerts on/off and its command topic as
   siblings under the base), with consistent payload conventions for booleans.
2. **Given** a sensor that is disabled or not working, **When** readings are published, **Then**
   its values are omitted or explicitly marked invalid — never sent as zeros that look real.
3. **Given** the MQTT settings, **When** the user chooses which groups to publish (per sensor,
   safety, alerts, diagnostics) and how often, **Then** only those are published.
4. **Given** default settings, **When** readings are published, **Then** low-level diagnostics
   (UART counters, raw responses, timing) are not included.
5. **Given** Home Assistant, **When** discovery is enabled, **Then** the device announces its
   entities so they appear without hand-written YAML.

---

### User Story 3 - A predictable REST API (Priority: P2)

A script author can rely on status codes and response shapes without reading each handler.

**Why this priority**: Scripts and automations break on special cases.

**Independent Test**: Call every endpoint with good and bad input; success is always 2xx with the
documented body, failure always 4xx/5xx with `{"error": "..."}`.

**Acceptance Scenarios**:

1. **Given** any action endpoint, **When** it succeeds, **Then** it returns 2xx with one success
   shape; **When** it fails, **Then** 4xx/5xx with `{"error": "..."}` — never 200 with a failure
   flag.
2. **Given** disabled hardware, **When** `/api/sensors` or `/api/status` is read, **Then** that
   hardware's data is absent.
3. **Given** the REST reference, **When** compared with the device, **Then** every endpoint, field
   and status code matches.

---

### User Story 4 - Live streams (Priority: P3)

The dashboard and integrators get live data over WebSockets with the same schema as REST.

**Acceptance Scenarios**:

1. **Given** `/ws/sensors` and `/ws/status`, **When** connected, **Then** messages use the same
   schema as `/api/sensors` and `/api/status`, every 1 s and 2 s respectively.

### Edge Cases

- MQTT broker down: readings are skipped, retained state is republished on reconnect.
- Broker requires login: credentials are stored masked.
- Payload larger than the MQTT buffer: it must not be silently dropped.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Measurements MUST use one schema across REST, WebSocket and MQTT: same group and
  field names, units and meanings.
- **FR-002**: All payload keys MUST be camelCase, and no value MUST be published under more than
  one name.
- **FR-003**: Timestamps MUST be Unix seconds with a separate clock-valid flag (or `0` until the
  clock is set). Display strings (e.g. `/api/status` `time.iso`, local time with its offset) MAY sit
  alongside, next to an `epoch` field.
- **FR-004**: MQTT topics MUST form one documented hierarchy under a base topic, with readings,
  availability, safe/safety, alerts and alerts on/off as siblings; boolean payloads MUST follow one
  documented convention.
- **FR-005**: MQTT readings MUST include every user-facing measurement available over REST (incl.
  dew point, wind, sensor validity) and MUST NOT include diagnostics unless the user enables them.
- **FR-006**: Values from disabled or faulted sensors MUST NOT be emitted as if valid, on any
  interface.
- **FR-007**: Users MUST be able to choose which MQTT groups are published and the interval.
- **FR-008**: The device MUST support Home Assistant MQTT discovery, switchable in settings. *(Per
  spec 020 D-13, the Alerts switch is only announced while alerts can go out.)*
- **FR-009**: REST endpoints MUST use one success shape and HTTP status codes for failure, with
  `{"error": "..."}` bodies (`action-result.schema.json`; `tools/contract-check.py` sends harmless
  actions to a device and checks their answers).
- **FR-010**: REST and WebSocket payloads MUST omit data for disabled hardware.
- **FR-011**: The MQTT and REST/WebSocket documentation MUST match the device field-for-field.
  The JSON Schemas in `specs/016-demo-device-emulation/contracts/schemas/` are the source of truth;
  the prose contracts here describe intent and point at them.
- **FR-012**: The device's limits on simultaneous connections MUST be documented, and a client past
  the limit refused cleanly (see spec 011 FR-008). The live-update WebSockets MUST be capped (3 per
  endpoint) so they can't crowd out Alpaca requests, and `/api/status` MUST report the
  `connections` it holds (schema `status.schema.json`).

*Language (spec 023 FR-015):* none of these interfaces change with the UI language. Numbers are
JSON numbers with `.` decimals and no grouping, units and names are never translated, and
device-generated text (reasons, alert titles) is English for display only; clients key on the
machine fields (`reasonFlags`, alert `event`).

### Key Entities

- **Readings document**: timestamp, clock-valid flag, safe flag, and one group per sensor (light,
  sky quality, environment, IR/cloud, location, rain, wind) each with validity and age.
- **MQTT topic tree**: base, readings, availability, safe, safety, alerts, alerts/armed (+ /set).
- **Publish settings**: groups on/off, interval, diagnostics, HA discovery.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 0 measurements with different names or units between interfaces.
- **SC-002**: 0 duplicate (aliased) keys and 0 non-camelCase keys in default payloads.
- **SC-003**: 0 documented fields/topics missing from the device and 0 emitted fields/topics
  missing from the docs.
- **SC-004**: Default MQTT readings payload under 1.5 KB (no diagnostics).
- **SC-005**: Every REST failure returns a non-2xx status.

## Assumptions

- Renamed fields and topics ship as a clean break (pre-1.0 beta), listed in the release notes.
