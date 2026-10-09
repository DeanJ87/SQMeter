# Quickstart: verifying IPv6

Prerequisites: a dual-stack LAN (router sends IPv6 router advertisements), the spare device on
it, a Mac on the same network.

## Build and tests

```bash
pio test -e native -f test_net_address -f test_config_model
pio run -e esp32dev && pio run -e esp32dev-ble
(cd web && npx tsc --noEmit && npx vitest run && npm run build:demo)
.venv-quality/bin/python tools/quality/check.py
python3 tools/settings-deps/check.py && python3 tools/docs/diagrams.py
```

## On the device

1. Flash firmware and filesystem over HTTP (IPv4).
2. `curl -s http://<ipv4>/api/status | jq .wifi.ipv6` - lists a `link-local` address and, if
   the router advertises a prefix, a `global`/`unique-local` one.
3. `ping6 -c 3 <global>` and `ping6 -c 3 fe80::...%en0`.
4. `curl -g "http://[<global>]/api/status"` and `curl -g "http://[fe80::...%en0]/api/status"` -
   same document as over IPv4. Open `http://[<global>]/` in a browser: live readings update
   (WebSocket over IPv6).
5. `dns-sd -G v4v6 <hostname>.local` - A and AAAA answers.
6. Alpaca over IPv6: `curl -g "http://[<global>]/api/v1/safetymonitor/0/issafe"`.
7. IPv6 discovery: send `alpacadiscovery1` to `ff12::a1:2345` port 32227 from the Mac
   (`tools/alpaca-sim` or a short Python script) - reply `{"AlpacaPort":80}`.
8. Settings → Network → IPv6 off, restart: `wifi.ipv6.addresses` is empty, no AAAA,
   `ping6` fails, everything over IPv4 still works.
9. MQTT to an IPv6 literal broker on the LAN connects (needs a broker; otherwise check the MQTT
   test reports a TCP-level result for `[<mac-ipv6>]`).
10. Heap and `stackFree` in `/api/status` with IPv6 on versus before.

## Results (2026-10-09)

Spare device (standard build), WiFi "Pretty Fly For a WiFi", 192.168.1.128. The Mac running the
checks is on another subnet (192.168.4.x) with no IPv6 route to it, so checks that need an IPv6
client on the device's own subnet are still open.

| Check | Result |
|---|---|
| IPv6 starts (serial log) | "Got IPv6 address: fe80::eb8:15ff:fe77:b16c (link-local)" ~0.8 s after the IPv4 address |
| SLAAC | `/api/status` lists `fe80::eb8:15ff:fe77:b16c` (link-local) and `fd4e:7e7a:e6ea:c59b:eb8:15ff:fe77:b16c` (unique-local) |
| IPv4 unchanged | Web UI, `/api/status`, contract check: all 12 endpoints ok against the device |
| Alpaca over IPv4 | discovery (unicast to 32227 through the dual-stack socket) answers `{"AlpacaPort":80}`; `issafe` answers |
| HTTP firmware upload | full speed on this build (2 of 3 first tries, one weak-signal reset at -77 dBm); with AsyncTCP 3.4.10: 0 of 6 (rejected, research R2) |
| Memory | free heap 178 KB, min free 153 KB, stack free: asyncTcp 6.3 KB, loop 2.6 KB (main: loop 2.6 KB) |
| Flash | standard 1,469,789 B (+15 KB), BLE 1,698,725 B (+15 KB) |
| Settings survive | config unchanged apart from the new `wifi.ipv6: true` |
| IPv6 off (FR-009) | `wifi.ipv6: false` + restart: `addresses: []`, contract check 12/12 ok; back on + restart: both addresses back |
| ConformU | not run this time: the macOS app opened its UI instead of running headless. The Alpaca handlers are unchanged by this spec; only the discovery socket is, and discovery answers (above). Run ConformU against both devices before release. |

Still to check with a dual-stack client on the device's subnet (quickstart steps 3-7, 9):
web UI and WebSocket over IPv6, `<hostname>.local` AAAA answers, the 403 for a peer outside the
prefix, Alpaca discovery to `ff12::a1:2345`, MQTT to an IPv6 broker.
