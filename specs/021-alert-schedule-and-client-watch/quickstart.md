# Quickstart: verify spec 021

## Native logic

```bash
pio test -e native -f test_alert_schedule -f test_alpaca_logic -f test_alert_logic -f test_config_logic
```

Expect: silence → `client_lost` once; a request → `client_back`; clean disconnect →
`client_disconnected`, no `client_lost`; restart without a request → nothing; two clients; cooldown;
mode/pause transitions; `armWithAlpaca` migration both ways; full templates fit.

## Firmware

```bash
pio run -e esp32dev && pio run -e esp32dev-ble
```

## Web and demo

```bash
cd web && npx tsc --noEmit && npx vitest run
npm run build:demo && npx playwright test -c playwright.local.config.ts tests/demo.spec.ts
```

In the demo: Demo panel → Imaging app → Connect, then Go silent, with 10× on. Within a minute an
"Imaging app stopped checking" alert appears under the bell; Resume checking → "Imaging app is
back"; set When to send = Only while an imaging app is connected, then Disconnect → status line
"Paused - the imaging app disconnected at HH:MM."

## On a device (not run by the builder)

1. Settings → Alerts: When to send = Only while an imaging app is connected; ntfy on.
2. Connect N.I.N.A. (or ConformU) to the SafetyMonitor; the status line says alerts are being sent.
3. Kill N.I.N.A.: within 2 min + 10 s an urgent "Imaging app stopped checking" arrives.
4. Restart N.I.N.A.: "Imaging app is back".
5. Disconnect normally: alerts pause; no "stopped checking".
6. `curl -X POST http://<device>/api/alerts/disarm` → status line "Paused from a script at …".
7. ConformU against both devices: zero errors.
