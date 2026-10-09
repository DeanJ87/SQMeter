# Feature Specification: ASCOM Alpaca Devices

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the native Alpaca SafetyMonitor and ObservingConditions devices: discovery, management API, device API, setup pages, the Alpaca web page and conformance."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - N.I.N.A. finds and uses the device (Priority: P1)

The imager opens N.I.N.A., refreshes the Alpaca device list, and the SQMeter SafetyMonitor and
ObservingConditions appear and connect, with no driver or bridge.

**Why this priority**: This is how the device gets used during imaging.

**Independent Test**: On the same LAN, N.I.N.A.'s discovery lists both devices on the first
refresh, and they connect.

**Acceptance Scenarios**:

1. **Given** Alpaca is enabled, **When** a client broadcasts discovery on UDP 32227, **Then** the
   device answers with its HTTP port promptly enough for the client's discovery window.
2. **Given** a client, **When** it reads the management API, **Then** both devices are listed
   with stable, unique IDs that include the board's MAC address.
3. **Given** Alpaca is disabled, **When** a client calls any endpoint, **Then** it gets a valid
   NotConnected reply rather than a 404, and the devices aren't listed.

---

### User Story 2 - Standards-conformant behaviour (Priority: P1)

Any Alpaca client works because the devices follow the specification exactly.

**Why this priority**: Clients differ; conformance is what makes all of them work.

**Independent Test**: ASCOM ConformU (conformance and protocol checks, both devices) reports
zero errors, issues and configuration alerts — in CI and against a real device.

**Acceptance Scenarios**:

1. **Given** GET requests, **When** parameter names use any casing, **Then** they're accepted;
   **Given** PUT requests, **When** names are wrongly cased, **Then** they're treated as missing.
2. **Given** an unknown device, number, method or verb, **When** requested, **Then** HTTP 400 with
   a plain-text message.
3. **Given** Platform 7 methods (Connect, Disconnect, Connecting, DeviceState), **When** called,
   **Then** they behave per the interface version advertised (SafetyMonitor 3, ObservingConditions 2).

---

### User Story 3 - Weather data per property (Priority: P2)

ObservingConditions serves each property from its sensor; a faulted sensor only affects its own
properties.

**Acceptance Scenarios**:

1. **Given** the documented property table, **When** each property is read, **Then** the source,
   units and NotImplemented/error behaviour match it.
2. **Given** `SensorDescription`/`TimeSinceLastUpdate`, **When** called with a property name,
   **Then** the serving sensor and its age are returned.

---

### User Story 4 - Setup and inspection (Priority: P3)

The Setup button in a client opens the device's own settings; the web UI's Alpaca page shows what
clients will see.

**Acceptance Scenarios**:

1. **Given** a client's Setup button, **When** clicked, **Then** the browser opens the Alpaca
   settings (Settings → Safety → ASCOM Alpaca).
2. **Given** the Alpaca page, **When** opened, **Then** each device's type, number, unique ID,
   setup URL, API base and live DeviceState (every 5 s) are shown, plus host/port for manual add.

### Edge Cases

- Discovery doesn't cross subnets/VLANs: manual add by IP and port 80 must work.
- Device IDs changed in v0.2.0: clients re-select once (release notes and docs say so).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST answer Alpaca discovery (UDP 32227) with its HTTP port, promptly
  (well within one second) regardless of what the main loop is doing.
- **FR-002**: The device MUST serve the management API (API versions, description, configured
  devices) and the SafetyMonitor and ObservingConditions device APIs from one implementation used
  by both the firmware and the CI simulator.
- **FR-003**: Parameter handling, transaction IDs, error numbers and HTTP 400s MUST follow the
  Alpaca specification.
- **FR-004**: ObservingConditions MUST serve each property from its own sensor with the documented
  units; unavailable hardware returns NotImplemented, a faulted/stale sensor returns a driver error.
  "Unavailable" includes a sensor that wasn't detected at boot, or is switched on but hasn't
  answered since boot; "faulted/stale" means it answered at least once and then stopped. The value,
  `timesincelastupdate` and `sensordescription` agree for every property.
- **FR-005**: Setup URLs (`/setup`, `/setup/v1/<type>/0/setup`) MUST open the Alpaca settings.
- **FR-006**: CI MUST run ConformU against the simulator on every change to the Alpaca code and
  fail on any error, issue or configuration alert.
- **FR-007**: Enabling Alpaca and its restart requirement for discovery MUST be explained in the
  UI (restart prompt) and the documentation, using the UI's current names.

### Key Entities

- **Alpaca device**: type, number, name, unique ID, connected state, interface version.
- **Management description**: server name, manufacturer, version, location.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: ConformU reports 0 errors, 0 issues and 0 configuration alerts for both devices,
  conformance and protocol.
- **SC-002**: Discovery replies arrive within 200 ms of the request on the local network.
- **SC-003**: N.I.N.A. lists both devices on the first discovery refresh.
- **SC-004**: Every UI name used in the Alpaca documentation exists in the UI.

## Assumptions

- Alpaca has no authentication (per the specification); the device is on a trusted LAN.
