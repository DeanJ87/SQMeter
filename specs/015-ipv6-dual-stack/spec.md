# Feature Specification: IPv6 (Dual Stack)

**Feature Branch**: `feat/015-ipv6`

**Created**: 2026-10-08

**Status**: Implemented (amended 2026-10-09 - FR-006 scope, see research R6)

**Input**: User description: "IPv6 support (dual-stack). SQMeter should work on IPv6 networks, not only IPv4: get IPv6 addresses (link-local and SLAAC global) on WiFi, serve the web UI, REST API, WebSockets and ASCOM Alpaca over IPv6, answer mDNS for <hostname>.local with AAAA records so names resolve without IPv4-only fallbacks or 5-second lookup stalls, connect outbound over IPv6 where the network offers it (MQTT broker, Pushover/ntfy/webhook alerts, GitHub update checks, NTP), and support Alpaca discovery over IPv6 (ff12::00a1:2345, port 32227). It must stay fully working on IPv4-only networks, be configurable (on by default if reliable, with an off switch like mDNS), show the device's IPv6 addresses, and never advertise an IPv6 address for a service that doesn't listen on IPv6."

## Clarifications

### Session 2026-10-08

- Q: A global IPv6 address can make the device reachable from the internet if the router allows it - refuse requests from outside the LAN, or rely on the router? → A: Accept IPv6 connections only from the local network (link-local and the device's own on-link prefixes); remote access stays a VPN/proxy job.
- Q: Must the device work on IPv6-only networks? → A: No - dual stack only (IPv6 alongside working IPv4). IPv6-only networks are a later spec.

### Amendment 2026-10-09 (planning)

- Arduino-ESP32 2.0.17's TLS client and SNTP only open IPv4 sockets (research R6). TLS and NTP over IPv6 need
  the Arduino-ESP32 3.x / ESP-IDF 5 platform upgrade, which is its own spec (it changes the constitution's
  Platform Constraints, every library pin and both image budgets). FR-006 is narrowed to plain-TCP
  outbound on this platform; every TLS/NTP service in scope is reachable over IPv4, so nothing regresses.
- The web server's async TCP library only listened on IPv4; it moves to AsyncTCP 3.4.10, which listens on
  both families (research R2). The assumption below that "the web server listens on all address families"
  was wrong for the pinned library.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reach the device over IPv6 on my LAN (Priority: P1)

On a home network that offers IPv6 (most routers now do, alongside IPv4), the owner opens the
device by name or address and the web UI, REST API, live updates and N.I.N.A. all work, whichever
protocol the computer picks.

**Why this priority**: It's the core of "supports IPv6" and fixes a visible problem: computers that
ask for the device's IPv6 address get none and either wait or fall back, and a device that only
works over IPv4 is the behaviour this feature exists to end.

**Independent Test**: On a dual-stack LAN, open the dashboard using the device's IPv6 address
and by `<hostname>.local`; confirm live readings update, settings save, and a client forced to
IPv6 succeeds.

**Acceptance Scenarios**:

1. **Given** IPv6 is on and the network offers it, **When** a computer looks up `<hostname>.local`,
   **Then** it gets the device's IPv6 address(es) as well as its IPv4 address.
2. **Given** a client that connects only over IPv6, **When** it opens the web UI, REST API or the
   live-update stream, **Then** they work exactly as over IPv4.
3. **Given** N.I.N.A. (or any Alpaca client) on IPv6, **When** it connects to the device's IPv6
   address, **Then** SafetyMonitor and ObservingConditions work.
4. **Given** any address the device advertises (by name lookup or discovery), **When** a client uses
   it, **Then** the service behind it answers - no address is advertised for a service that doesn't.

---

### User Story 2 - Nothing breaks on IPv4-only networks (Priority: P1)

An owner whose network has no IPv6, or who switches IPv6 off, sees no change from today.

**Why this priority**: Most existing installs must keep working; a regression here is worse than
not having IPv6.

**Independent Test**: On an IPv4-only network, and separately with IPv6 switched off, run the
existing checks (web UI, REST, WebSockets, MQTT, alerts, updates, Alpaca discovery and ConformU)
and compare with the previous release.

**Acceptance Scenarios**:

1. **Given** a network with no IPv6, **When** the device runs, **Then** every feature works as before
   and no lookup or connection waits on IPv6.
2. **Given** IPv6 switched off in settings, **When** the device restarts, **Then** it has no IPv6
   address and advertises none.

---

### User Story 3 - Outbound services over IPv6 (Priority: P2)

The device reaches its MQTT broker, alert services (Pushover, ntfy, webhooks), GitHub for
updates and time servers over IPv6 when that's what the network or the server offers.

**Why this priority**: Needed for brokers or webhooks that only have IPv6 addresses, and for
future IPv6-only networks; most users' services are reachable over IPv4 today.

**Independent Test**: Point MQTT and a webhook at IPv6-only addresses/hostnames on a dual-stack
LAN and confirm publishing and alert delivery; run an update check.

**Acceptance Scenarios**:

1. **Given** an MQTT broker or webhook given as an IPv6 address or a name that only has an IPv6
   address, **When** the device connects, **Then** it succeeds.
2. **Given** a server reachable over both, **When** IPv6 to it fails, **Then** the device falls back
   to IPv4 without the user noticing beyond a short delay.

---

### User Story 4 - N.I.N.A. finds the device over IPv6 (Priority: P3)

Alpaca clients that discover devices over IPv6 find SQMeter without typing an address.

**Why this priority**: Clients discover over IPv4 today; IPv6 discovery completes the Alpaca
picture but isn't required to use the device.

**Independent Test**: Send an Alpaca discovery request to the IPv6 discovery group and check the
reply; run ConformU's discovery tests.

**Acceptance Scenarios**:

1. **Given** IPv6 is on, **When** a client sends an Alpaca discovery request to the IPv6 discovery
   group, **Then** the device answers with its Alpaca port, as it does over IPv4.

---

### User Story 5 - See and control IPv6 (Priority: P2)

The owner can see the device's IPv6 addresses and switch IPv6 off.

**Why this priority**: Needed to troubleshoot and to opt out on networks where IPv6 misbehaves.

**Independent Test**: Read the addresses in the web UI and API; toggle the setting and confirm the
effect after restart.

**Acceptance Scenarios**:

1. **Given** IPv6 is on, **When** the owner opens the device's status, **Then** it lists each IPv6
   address with its kind (link-local or global).
2. **Given** the settings, **When** the owner switches IPv6 off and restarts, **Then** IPv6 is off.

---

### Edge Cases

- The network offers IPv6 link-local only (no router advertisement): link-local works on the LAN;
  outbound stays IPv4.
- The global prefix changes (ISP renumbering) or an address expires: the device picks up the new
  address and stops advertising the old one.
- A network with no IPv4 at all (IPv6-only, possibly with NAT64/DNS64): out of scope (FR-012); the
  device behaves as it does today with no IPv4 (setup hotspot).
- The setup hotspot: stays IPv4 (phones join it with IPv4); IPv6 applies to the joined network.
- A host name with both address families where IPv6 is unreachable (broken upstream IPv6):
  connections fall back to IPv4 promptly (FR-008).
- mDNS turned off: no AAAA (or A) is announced; IPv6 still works by address.
- Literal IPv6 addresses entered in settings (MQTT broker, webhook URL) are accepted in the usual
  forms (`fd00::10`, `[fd00::10]:1883`, `http://[fd00::10]/hook`).
- A request arrives on a global IPv6 address from outside the LAN: refused (FR-011).
- The on-link prefix changes: the allowed range follows the device's current prefixes.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: When IPv6 is on, the device MUST obtain an IPv6 link-local address on the joined WiFi
  network, and a global or unique-local address by stateless autoconfiguration when the network
  advertises a prefix.
- **FR-002**: The web UI, REST API, WebSocket streams and Alpaca HTTP API MUST be reachable on every
  IPv6 address the device holds, with the same behaviour and authentication as over IPv4.
- **FR-003**: With mDNS on, `<hostname>.local` lookups MUST return the device's current IPv6 address(es)
  alongside its IPv4 address, and the advertised web service MUST be reachable on them.
- **FR-004**: The device MUST NOT advertise (by name lookup or discovery) an address on which the
  advertised service doesn't accept connections.
- **FR-005**: The device MUST answer Alpaca discovery requests sent to the Alpaca IPv6 discovery group,
  in addition to IPv4 broadcast.
- **FR-006**: Plain-TCP outbound connections (MQTT and `http://` webhooks) MUST work to IPv6 addresses and to
  names that resolve only to IPv6. TLS clients (Pushover, ntfy over https, https webhooks, GitHub update
  checks and downloads) and NTP stay IPv4 until the platform-upgrade spec (amendment 2026-10-09); settings
  MUST say so where they'd otherwise accept an IPv6 address that can't work (an https URL with an IPv6
  literal).
- **FR-007**: Settings MUST accept IPv6 literals wherever a host or URL is entered, and validate them
  in the browser and on the device with the same rules.
- **FR-008**: Broken upstream IPv6 MUST never stop alerts, MQTT or updates. Names with both IPv4 and IPv6
  addresses are reached over IPv4 (the platform resolver prefers A), so no IPv6 attempt is made for them.
- **FR-009**: IPv6 MUST be switchable (default on); with it off the device MUST behave exactly as an
  IPv4-only device.
- **FR-010**: The status API and the System page MUST show each current IPv6 address and its scope
  (link-local, unique-local, global).
- **FR-011**: Over IPv6 the device MUST accept connections (web UI, API, WebSockets, Alpaca) only from
  link-local addresses and addresses within its own on-link prefixes, and refuse others, so a
  permissive router firewall can't put it on the internet. IPv4 behaviour is unchanged.
- **FR-012**: The scope is dual stack: IPv6 working alongside IPv4. Networks without IPv4 (IPv6-only,
  NAT64/DNS64, DNS from router advertisements or DHCPv6) are out of scope and left to a later spec.
- **FR-013**: Everything that works on an IPv4-only network today MUST keep working with IPv6 on or off,
  including ConformU against both Alpaca devices.
- **FR-014**: The docs MUST describe IPv6 behaviour, the setting, and how to reach the device by IPv6.

### Key Entities

- **IPv6 setting**: on/off, part of the network settings (alongside mDNS).
- **Device IPv6 address**: address, scope (link-local / unique-local / global), shown in status.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a dual-stack LAN, a client forced to IPv6 completes every web UI and REST check the
  IPv4 test suite runs, with zero failures.
- **SC-002**: On a dual-stack LAN, resolving `<hostname>.local` from macOS, Windows and Linux returns
  IPv6 and IPv4 answers and the dashboard opens without a multi-second lookup wait caused by the device.
- **SC-003**: On an IPv4-only network and with IPv6 switched off, ConformU and the existing test
  checklist pass with the same results as the previous release.
- **SC-004**: MQTT publishing and an alert reach an IPv6-only broker/webhook on the LAN on the first try.
- **SC-005**: With upstream IPv6 deliberately broken, alerts and MQTT still get through within
  10 seconds of the normal time.
- **SC-007**: A request to the device's global IPv6 address from outside its network prefix is refused.
- **SC-006**: Both firmware images still fit their update slots and the device keeps at least the
  current free-memory headroom with IPv6 on.

## Assumptions

- The device stays on WiFi; Ethernet is out of scope.
- The setup hotspot stays IPv4-only (it's only used to enter WiFi details).
- DHCPv6 (stateful addressing) is not required; stateless autoconfiguration is what home routers use.
- Investigation 2026-10-08: the current platform (Arduino-ESP32 2.0.17 / ESP-IDF 4.4) already
  autoconfigures IPv6 and its mDNS answers AAAA once a link-local address exists; the web server
  listens on all address families. The outbound clients (MQTT, HTTP/TLS) in this platform version
  connect by IPv4 address only, so FR-006/FR-008 may need a platform upgrade (Arduino-ESP32 3.x /
  ESP-IDF 5). That is a planning decision and would amend the constitution's Platform Constraints;
  the plan should weigh delivering P1/P2 inbound stories first.
- On the maintainer's Mac every `.local` lookup currently waits 5 s for an IPv6 answer, for all
  devices on the network; answering AAAA is expected to help on networks like it, but the wait is
  partly down to the client and network.

## Dependencies

- Constitution IV (resource budgets): IPv6 adds memory per address and connection; must be measured.
- Constitution VI (trusted-LAN security): global IPv6 addresses change the exposure picture (FR-011).
- Spec 014 (WiFi setup and mDNS), 007 (ASCOM Alpaca discovery), 008 (alerts), 012 (updates),
  013 (MQTT) - each gains IPv6 behaviour.
