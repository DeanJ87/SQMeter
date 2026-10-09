# Research: IPv6 (Dual Stack)

Investigated 2026-10-09 against the pinned platform: Arduino-ESP32 2.0.17
(`framework-arduinoespressif32` 3.20017.241212, ESP-IDF 4.4), ESPAsyncWebServer 3.6.0,
AsyncTCP (pinned git commit ef448a8 = 3.3.2), PubSubClient 2.8.

## R1. Addresses (FR-001)

- **Decision**: call `WiFi.enableIpV6()` on `ARDUINO_EVENT_WIFI_STA_GOT_IP` when the
  `wifi.ipv6` setting is on. On the device, calling it at `STA_CONNECTED` failed ("IPv6 failed to
  start"): the interface isn't up yet then. That creates the link-local address; SLAAC global / unique-local
  addresses follow from router advertisements without further code.
- **Rationale**: the SDK is built with `CONFIG_LWIP_IPV6=y`, `CONFIG_LWIP_IPV6_AUTOCONFIG=y`
  and `CONFIG_LWIP_IPV6_NUM_ADDRESSES=3`; esp-lwip sets `ip6_autoconfig_enabled` when autoconfig
  is compiled in. `WiFi.enableIpV6()` only creates the link-local address
  (`esp_netif_create_ip6_linklocal`) and needs the interface up.
  `WiFi.localIPv6()` returns the link-local address only, so the status reads all addresses
  with `esp_netif_get_all_ip6()`.
- **Limits**: 3 addresses per interface (link-local + 2 SLAAC); DHCPv6 isn't compiled in
  (`CONFIG_LWIP_IPV6_DHCP6` unset) - matches the spec's assumption that SLAAC is what home
  routers use. Router-advertised DNS (RDNSS) is off (`RDNSS_MAX_DNS_SERVERS=0`) - fine for dual
  stack, out of scope per FR-012.
- **Off**: with the setting off nothing creates a link-local address, so the stack never
  autoconfigures and mDNS never answers AAAA (FR-009).

## R2. Inbound HTTP, WebSockets, Alpaca (FR-002) - a second listener

- **Finding**: the spec's assumption "the web server listens on all address families" is
  **wrong for the pinned AsyncTCP**. AsyncTCP 3.3.2's `AsyncServer(port)` sets `_bind4=true,
  _bind6=false` and on ESP-IDF < 5 creates an `IPADDR_TYPE_V4` listening pcb ("_bind6 &&
  _bind4 both at the same time is not supported on Arduino 2 in this lib API"). The web UI was
  IPv4-only.
- **Tried and rejected: AsyncTCP 3.4.10** (registry), whose `AsyncServer(port)` listens on
  `IPADDR_TYPE_ANY`. On the spare device it broke HTTP firmware uploads: 6 of 6 full-speed
  `POST /api/update` uploads were reset mid-transfer (curl 55); a 30 KB/s throttled one got
  through. The same uploads on 3.3.2 succeed. A broken update path is worse than IPv4-only web
  access, so the pin stays at 3.3.2.
- **Decision**: keep AsyncTCP 3.3.2 and add a second, IPv6-only `AsyncServer(IPv6Address(), 80)`
  whose accepted connections become `AsyncWebServerRequest`s of the same `AsyncWebServer` (as
  its own listener does). lwIP allows an IPv4 and an IPv6 listener on one port. Every handler,
  WebSocket and Alpaca route is served on both.
- **Alternatives**: patching the library (fragile); upgrading to Arduino-ESP32 3.x (see R6).

## R3. LAN-only over IPv6 (FR-011, Constitution VI)

- **Decision**: checked when the IPv6 listener accepts a connection. Only IPv6 connections reach
  it, so IPv4 can't be affected. The peer must be link-local (`fe80::/10`), loopback, or share a
  /64 with one of the device's own unique-local or global addresses; otherwise the device writes
  `403 IPv6 requests are only accepted from the local network` and closes before reading a
  request. The decision is pure logic in `lib/NetAddress` with native tests.
- **Why not middleware**: the first version used an ESPAsyncWebServer middleware. Two problems
  showed on the device:
  - AsyncTCP 3.3.2's `getRemoteAddress6()` returns the address union whatever the family, so
    IPv4 peers looked like IPv6 and every IPv4 request got 403.
  - Middleware runs after the body is parsed, but `/api/update`'s upload handler writes and
    activates firmware as the data arrives - an off-LAN IPv6 peer could have pushed firmware even
    though its request was refused. Refusing at accept closes both.
- **Rationale**: SLAAC prefixes are always /64; the allowed set follows the device's current
  addresses, so renumbering moves it automatically.
- **Discovery**: IPv6 discovery joins `ff12::a1:2345` - a link-scope multicast group (scope 2),
  so routers never forward it; replies only go back to the link-local sender.

## R4. mDNS AAAA (FR-003, FR-004)

- **Finding**: ESP-IDF 4.4's mdns component builds with IPv6 (`CONFIG_LWIP_IPV6`) and enables
  its IPv6 PCB on `IP_EVENT_GOT_IP6`, answering AAAA for the hostname and the `_http._tcp`
  service once the interface has a link-local address. With IPv6 off no AAAA is ever answered.
- **FR-004**: AAAA is only answered when the interface has IPv6, and from R2 the HTTP service
  then listens on IPv6 too.

## R5. Alpaca discovery over IPv6 (FR-005)

- **Decision**: with IPv6 on, the discovery socket listens on `IP_ANY_TYPE` port 32227, so one
  socket receives IPv4 broadcasts and IPv6 datagrams and answers each with the same
  `Alpaca::buildDiscoveryResponse` (`IP_SOF_BROADCAST_RECV` is 0 on this platform, so an
  any-type socket accepts broadcasts exactly as the IPv4 one did). The device joins the
  `ff12::a1:2345` group with `mld6_joingroup` on the TCP/IP task once it has an IPv6 address
  (AsyncUDP's own `listenMulticast` binds an IPv4 socket, so it can't receive IPv6). With IPv6
  off the socket is IPv4-only as before.

## R6. Outbound (FR-006, FR-008) - partly not achievable on this platform

- **Finding**: in Arduino-ESP32 2.0.17
  - `WiFiClient::connect(host)` resolves with `hostByName` (IPv4 only) and opens an `AF_INET`
    socket; `IPAddress` is IPv4-only.
  - `WiFiClientSecure` (`ssl_client.cpp`) also opens `AF_INET` sockets, so **all TLS clients**
    (Pushover, ntfy over https, https webhooks, GitHub update checks and downloads) are
    IPv4-only.
  - `HTTPClient` splits `http://[fd00::10]:8080/x` at the first `:`, so IPv6 literals in URLs
    break it.
  - SNTP resolves names with the default IPv4-first lookup.
- **Decision**:
  - A `DualStackClient` (a `WiFiClient` subclass in `src/`) resolves with `lwip_getaddrinfo`
    (`AF_UNSPEC`) or takes an IPv6 literal (with or without brackets), and opens an
    `AF_INET6` socket for IPv6 results, handing the connected socket to `WiFiClient`. It's used
    for MQTT, the MQTT test and plain `http://` webhooks - every plain-TCP outbound client.
  - Webhook URLs are parsed by `lib/NetAddress` (bracket-aware) and passed to
    `HTTPClient::begin(client, host, port, uri, https)` with the host bracketed, so the `Host:`
    header is valid.
  - lwIP's `getaddrinfo` returns one address and prefers A over AAAA, so names with an IPv4
    address keep using IPv4 and **IPv6 is used only for IPv6 literals and IPv6-only names**.
    Broken upstream IPv6 therefore never delays MQTT or alerts (FR-008 holds by construction),
    and SC-005 is met with no added delay.
- **Not achievable without a platform upgrade (spec amendment)**: TLS over IPv6 (Pushover, ntfy
  over https, https webhooks, GitHub updates) and NTP over IPv6 need Arduino-ESP32 3.x /
  ESP-IDF 5 (its `NetworkClient`/`NetworkClientSecure` are dual stack). That upgrade changes the
  constitution's Platform Constraints, every library pin, both image sizes and the BLE stack, so
  it's its own spec. All those services are dual-stack today, so they keep working over IPv4.
  The spec is amended: FR-006 covers plain-TCP outbound (MQTT, http webhooks) on this platform,
  and TLS/NTP over IPv6 move to the platform-upgrade spec.

## R7. Settings and validation (FR-007, FR-009)

- **Decision**: `wifi.ipv6` (bool, default true) beside `wifi.mdns`; applies at restart (the
  Settings page lists it among "Restart to apply").
- **Validation** (`lib/NetAddress` + `web/src/lib/netAddress.ts`, one shared fixture file):
  - MQTT broker: a host name, an IPv4 address, or an IPv6 literal, bare (`fd00::10`) or
    bracketed (`[fd00::10]`), optionally `[fd00::10]:1883` - then that port is used. Anything
    containing `:` that isn't one of those is rejected ("Not a valid IPv6 address" / "Put IPv6
    addresses in brackets to add a port"). Zone IDs (`%wlan0`) are rejected - the device has one
    interface.
  - Webhook URL: `http://` or `https://`, then a host as above (IPv6 bracketed), optional port
    1-65535, optional path. https with an IPv6 literal is rejected with the platform reason
    (R6), so the setting never looks valid when it can't work.
  - Existing saved values that pass today's looser check but fail the new one mustn't stop the
    config loading, so - like the Pushover key format - the new rules are enforced only for
    changes coming from the UI/API (`applyJson` with `preserveSecretPlaceholders`), never when
    loading saved config (Platform Constraints: Compatibility).
- **Settings dependencies (spec 020)**: no new dependency: IPv6 discovery is part of Alpaca
  discovery (already under Alpaca), AAAA is part of mDNS. Nothing becomes "inactive".

## R8. Status (FR-010)

- `/api/status` → `wifi.ipv6 = { enabled, addresses: [{ address, scope }] }`, scope one of
  `link-local`, `unique-local`, `global`. Addresses formatted per RFC 5952 by `lib/NetAddress`.
  The System page lists them under the IPv4 address.

## R9. Memory

- lwIP's IPv6 pools are already compiled in; the cost is per address/neighbour (ND6 tables
  sized 5 neighbours, 3 queued packets) and one more UDP pcb. Measured on the spare device
  (see quickstart).

## R10. Device results (spare, 2026-10-09)

See [quickstart.md](quickstart.md#results-2026-10-09).
