# Data Model: IPv6 (Dual Stack)

## Setting

| Path | Type | Default | Applies | Notes |
|---|---|---|---|---|
| `wifi.ipv6` | boolean | `true` | after restart | Missing in older saved config → `true`. Off: no link-local address is created, so no SLAAC, no AAAA, no IPv6 discovery. |

## Device IPv6 address (status)

`/api/status` → `wifi.ipv6`:

```json
{ "enabled": true, "addresses": [
  { "address": "fe80::a00:27ff:fe4e:66a1", "scope": "link-local" },
  { "address": "2a02:8010:abcd:1:a00:27ff:fe4e:66a1", "scope": "global" }
] }
```

- `enabled`: the setting the device is running with.
- `addresses`: valid addresses on the station interface, RFC 5952 text, at most 3; empty when
  IPv6 is off or the device is in setup-hotspot mode.
- `scope`: `link-local` (fe80::/10), `unique-local` (fc00::/7), `global` (everything else
  unicast).

## Host and URL values (validation)

| Field | Accepted | Rejected (message) |
|---|---|---|
| `mqtt.broker` | host name; IPv4; IPv6 bare `fd00::10` or bracketed `[fd00::10]`; `[fd00::10]:1883` (port used) | bad IPv6 ("Not a valid IPv6 address"); bare IPv6 with a port or `host:port` ("Put IPv6 addresses in brackets to add a port" / "Put the port in the Port field"); zone id ("Leave out the %zone - the device has one network interface") |
| `alerts.webhook.url` | `http(s)://host[:port][/path]`, IPv6 bracketed | missing scheme; unbracketed IPv6; port outside 1-65535; `https://` with an IPv6 literal ("https to an IPv6 address isn't supported yet - use a name or http") |

Rules live in `lib/NetAddress` and `web/src/lib/netAddress.ts`, held together by
`test/fixtures/net-address/cases.json`. Enforced for changes from the UI/API only.

## Peer policy (FR-011)

`allowedPeer(peer, own[])`: IPv4 (and IPv4-mapped) → allowed; `::1` → allowed; link-local →
allowed; otherwise allowed only if the first 64 bits equal those of one of the device's own
unique-local or global addresses.
