# Feature Specification: WiFi Setup and Network Presence

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill first boot: the setup hotspot and captive portal, joining WiFi, finding the device on the network, and reconnecting."

## Clarifications

### Session 2026-10-08

- Q: Should mDNS be added? → A: Yes, and it must be configurable.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Get it on my WiFi (Priority: P1)

A new owner powers the device, joins its open "SQM-Setup" hotspot, is taken straight to a page
listing nearby networks, picks theirs, enters the password and connects.

**Why this priority**: Nothing else works until this does.

**Independent Test**: From a factory-fresh device, a phone completes setup without typing an IP
address or knowing the web UI's layout.

**Acceptance Scenarios**:

1. **Given** no saved WiFi, **When** the device boots, **Then** it broadcasts the open "SQM-Setup"
   hotspot with a captive portal.
2. **Given** a phone joins, **When** the OS opens the sign-in page, **Then** it lands on WiFi
   setup (network list with signal and lock, password, Connect), not the dashboard.
3. **Given** valid credentials, **When** Connect is pressed, **Then** the device joins, the hotspot
   closes and the page says where to find the device next.

---

### User Story 2 - Find it afterwards (Priority: P1)

The owner reaches the device by a stable name or its IP.

**Acceptance Scenarios**:

1. **Given** the device is on the network, **When** the user opens `http://<hostname>.local`,
   **Then** it loads (mDNS), using the hostname set in Settings (default as documented).
2. **Given** the documentation, **When** it shows example URLs, **Then** they use one hostname —
   the default.

---

### User Story 3 - Stay connected (Priority: P2)

The device reconnects after WiFi drops, backing off between attempts; the owner can change network
from Settings.

**Acceptance Scenarios**:

1. **Given** WiFi drops, **When** auto-reconnect is on, **Then** the device retries with
   exponential back-off up to the configured maximum.
2. **Given** Settings → Network, **When** the user scans and picks another network, **Then** the
   new network is saved and applied with a restart prompt.

### Edge Cases

- Wrong password: the user is told, and the hotspot stays available.
- 5 GHz-only networks: not listed (2.4 GHz only).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Without saved WiFi the device MUST start the open "SQM-Setup" hotspot with DNS-based
  captive portal detection for iOS, Android, macOS and Windows.
- **FR-002**: The captive portal MUST land on a WiFi setup screen (scan, choose, password, Connect).
- **FR-003**: The device MUST advertise itself via mDNS as `<hostname>.local` (and advertise its
  HTTP service), and users MUST be able to turn mDNS off in settings.
- **FR-004**: The device MUST reconnect automatically with back-off when enabled.
- **FR-005**: Every API endpoint MUST be used by the UI or documented for integrators; unused
  endpoints MUST be removed.
- **FR-006**: The first-boot guide and examples MUST use the actual hotspot name, default hostname
  and UI labels.

### Key Entities

- **WiFi settings**: SSID, password (masked), hostname, auto-reconnect, back-off delays.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A first-time user completes WiFi setup from a phone in under 3 minutes without
  typing an IP address.
- **SC-002**: `http://<default hostname>.local` reaches the device on a typical home network.
- **SC-003**: 0 hostnames in the docs that differ from the device's default.

## Assumptions

- Default hostname is `sqm-esp32` (as configured), unless the maintainer chooses to change it.
