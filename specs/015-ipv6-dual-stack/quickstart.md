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
