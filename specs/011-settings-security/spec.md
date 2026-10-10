# Feature Specification: Settings, Configuration and Security

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Find and change a setting easily (Priority: P1)

The owner finds settings in six tabs (Device, Network, Time & Location, Sensors, Safety, Alerts);
options that depend on missing hardware are disabled with the reason and a link to fix it.

**Acceptance Scenarios**:

1. **Given** a setting that depends on absent/disabled hardware, **When** viewed, **Then** it can't
   be switched on and says why, with a link; if already on, it can be switched off.
2. **Given** unsaved changes, **When** present, **Then** a save bar appears; Discard restores.
3. **Given** a change that only applies after a restart, **When** saved, **Then** a toast names
   what needs the restart and offers Restart.

---

### User Story 2 - Settings that don't break the device (Priority: P1)

Invalid values are caught in the browser and again on the device; settings survive updates and
older saved settings are migrated.

**Acceptance Scenarios**:

1. **Given** an out-of-range value, **When** saving, **Then** the browser and the device enforce the
   same ranges, and the browser shows the error on the right tab.
2. **Given** settings saved by an older release, **When** the device boots, **Then** they load and
   are migrated without a factory reset.

---

### User Story 3 - Keep others out (Priority: P2)

The owner turns on password protection; changes, actions and anything revealing more than
readings need the password; secrets are never shown back.

**Acceptance Scenarios**:

1. **Given** protection on, **When** a client calls a protected endpoint without credentials,
   **Then** it gets 401; readings and Alpaca stay open.
2. **Given** any secret, **When** config is read, **Then** it is masked, and sending the mask back
   keeps the stored value.

### Edge Cases

- Config too large for NVS (custom alert texts): rejected with a clear message.
- Forgotten password: documented recovery by erasing NVS.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Settings MUST be organised in the six tabs; old section links MUST map to tabs.
- **FR-002**: Dependent options MUST be blocked with a reason and fix link when their dependency is
  missing. *(Superseded by 020: a dependent setting is kept but reported inactive, with the reason
  and a fix link; it can't be newly switched on while its dependency is off. Catalogue:
  `lib/SettingsDeps/catalogue.json`.)*
- **FR-003**: The save bar MUST appear only with unsaved changes; restart-requiring changes MUST be
  announced by toast with a Restart action, and only those.
- **FR-004**: Browser and device validation MUST enforce identical ranges, checked against one shared
  fixture (`test/fixtures/config-ranges.json`).
- **FR-005**: Config MUST persist in NVS within its limits and migrate from older formats; settings
  saved by each release are kept as fixtures (`test/fixtures/config-releases/`) and loaded in a test.
- **FR-006**: Password protection MUST cover every state-changing or action endpoint (including a
  WiFi scan, which starts a radio scan); secrets MUST be masked and preserved. Every route MUST be
  declared with its auth in `tools/api/routes.json`, checked in CI. ASCOM Alpaca routes are open by
  the Alpaca design.
- **FR-008**: The device MUST serve several clients at once (at least two browsers, an imaging app
  and MQTT) and, past its connection limit, refuse new connections cleanly: no restart, and
  existing clients keep working. The limit and behaviour MUST be documented. An imaging app MUST
  NOT lose its connection because of other clients or a restart the device caused itself:
  - Live-update sockets (`/ws/sensors`, `/ws/status`) MUST be capped at 3 clients per endpoint; a
    new client past the cap replaces the oldest. With MQTT and one outbound TLS connection this
    leaves at least 8 of the 16 TCP connections for HTTP, so an imaging app polling both Alpaca
    devices always gets one.
  - A live-update client that stops reading (its send queue full) MUST be closed within 10 s, and
    quiet clients MUST be pinged every 15 s so a peer that vanished is dropped. One stuck client
    MUST NOT delay updates to the others.
  - After a restart the device caused itself (software restart, crash, watchdog), an Alpaca device
    an imaging app had connected MUST still report `Connected = true`. A power-on, brownout or the
    reset button starts with nothing connected.
  - `/api/status` → `connections` MUST report the live-update clients per endpoint, the cap, the
    clients closed since the restart, and whether connections were kept across the last restart.

  *Acceptance:* (1) An imaging-app-like client polling both devices every 3 s, while sockets that
  are opened and never read are added every 20 s up to 24 at once, has zero failed requests over
  30 minutes (`tools/soak/connection_soak.py`). (2) A software restart while N.I.N.A. is connected
  leaves `GET …/safetymonitor/0/connected` `true` after the restart. *(Decisions:
  [research-connections.md](research-connections.md); see spec 013 FR-012.)*
- **FR-007**: The configuration and security documentation MUST match the device.

### Key Entities

- **Config**: all sections (device, wifi, ntp, gps, location, mqtt, ota, auth, rain, sensor, sky,
  cloud detection, alpaca, alerts, ble, wind).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 0 ranges that differ between browser and device validation.
- **SC-002**: 0 unprotected endpoints that change state.
- **SC-003**: Settings from every earlier release load without loss.

## Assumptions

- The device is on a trusted LAN; HTTP Basic Auth without TLS is acceptable.
