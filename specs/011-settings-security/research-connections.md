# Research: imaging apps keep their connection (FR-008)

Prompted by a user on v0.2.0-beta.3 whose N.I.N.A. lost the SafetyMonitor a few times a night,
with good WiFi and only the web UI and N.I.N.A. connected; reconnecting worked at once.

## How the device runs out of connections

- lwIP allows `CONFIG_LWIP_MAX_ACTIVE_TCP` = 16 TCP connections (`sdkconfig.esp32dev`).
- ESPAsyncWebServer answers every request with `Connection: close`, so each Alpaca request is a new
  connection. N.I.N.A. polls both devices every few seconds; it needs free ones all the time.
- Each web UI tab keeps two WebSockets (on v0.2.0-beta.3 the alerts bell opened a third).
  ESPAsyncWebServer allows 8 clients per endpoint by default (`DEFAULT_MAX_WS_CLIENTS`): two
  endpoints could take all 16.
- A tab in a browser that's asleep keeps its socket open but stops reading. Its send queue fills
  (`WS_MAX_QUEUED_MESSAGES` 4); further messages are dropped but the socket stays (the library's
  close-when-full is off by default), and AsyncTCP's 5 s acknowledgement timeout never fires because
  nothing more is sent. Each wake-up or reload adds two more.
- Worse, the broadcast skipped *every* client while any one was full (`availableForWriteAll`), so a
  single stuck tab froze live updates for all tabs and its own queue never filled further.
- Once all 16 are held, lwIP can't accept N.I.N.A.'s next connection; N.I.N.A. times out and
  marks the device disconnected. Freed later, reconnecting works.

A restart has the same symptom: the Alpaca `Connected` flag lives in memory, so after a crash or
watchdog reset the device answers `Connected = false` and N.I.N.A. reports it disconnected.

## Decisions

| Decision | Why | Rejected |
|---|---|---|
| Cap live updates at 3 per endpoint, oldest replaced | 3 tabs is plenty for one device; 6 + MQTT + TLS leaves 8 for HTTP | Raising `CONFIG_LWIP_MAX_ACTIVE_TCP`: costs heap per connection and only delays exhaustion; not measurable on a device at the time |
| Close a client whose queue stays full 10 s | A sleeping tab never drains; a weak-WiFi backlog drains within seconds | The library's close-when-full: closes on the first full queue, so weak-WiFi clients would reconnect in a loop |
| Ping quiet clients every 15 s | A vanished peer stops acknowledging; AsyncTCP then closes it in 5 s | Relying on TCP keepalive: off in lwIP by default, minutes long |
| Broadcast to every client; a full one just misses messages | One stuck tab must not stall the others | Skipping all while one is full (old behaviour) |
| Keep Alpaca connections across a software restart, crash or watchdog (RTC memory, checked) | The imaging app didn't change its mind; the device did | Persisting in NVS: flash wear on every connect, and it would survive power cycles too |
| Report `connections` in `/api/status`, not on the dashboard | Troubleshooting data (DS rules); the dashboard inventory records it as API-only | A System page card: no user action follows from the numbers |

## Verification

- Native tests: `test/test_connection_budget` (budget arithmetic, stall timing across the `millis()`
  wrap, which restarts keep connections, the RTC record's checks, restored connections read
  `Connected = true` without counting a disconnect).
- Device soak: `tools/soak/connection_soak.py` (spec acceptance 1). Run against a device on the
  current firmware; results are recorded in the PR.
