# Feature Specification: Settings, Configuration and Security

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill the tabbed, dependency-aware Settings, config persistence/validation/migration, restart handling, and password protection."

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
  missing.
- **FR-003**: The save bar MUST appear only with unsaved changes; restart-requiring changes MUST be
  announced by toast with a Restart action, and only those.
- **FR-004**: Browser and device validation MUST enforce identical ranges.
- **FR-005**: Config MUST persist in NVS within its limits and migrate from older formats.
- **FR-006**: Password protection MUST cover every state-changing or action endpoint; secrets MUST
  be masked and preserved.
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
