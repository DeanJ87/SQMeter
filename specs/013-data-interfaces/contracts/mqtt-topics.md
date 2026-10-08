# Contract: MQTT topics

`<base>` is the configured base topic (default `sqmeter`). Booleans are `1`/`0`; availability
uses Home Assistant's `online`/`offline`.

| Topic | Retained | Payload | When |
|---|---|---|---|
| `<base>/availability` | yes | `online` / `offline` (last will) | connect / disconnect |
| `<base>/state` | yes | Readings document (enabled groups) | every publish interval |
| `<base>/safe` | yes | `1` safe / `0` unsafe | on change, refreshed every minute (if safety group on) |
| `<base>/safety` | yes | `GET /api/safety` object | same |
| `<base>/alerts` | no | `{"event","events"?,"title","message","level","device","timestamp"}` | each alert (Alerts → MQTT) |
| `<base>/alerts/armed` | yes | `1` / `0` | on change and reconnect |
| `<base>/alerts/armed/set` | — | subscribed: `1`/`0`, `on`/`off`, `true`/`false` | command |
| `<base>/diagnostics` | no | `/api/status` → `diagnostics` | every publish interval (if diagnostics group on) |

## Publish settings (`mqtt` config)

- `topic`: base topic.
- `publishIntervalMs`: readings interval (1 s – 24 h).
- `publish`: `{ "sky", "environment", "clouds", "gps", "rain", "wind", "safety", "diagnostics" }`
  booleans — `sky` covers `light`+`sky`, `clouds` covers `infrared`+`clouds`. Defaults: all on
  except `diagnostics`.
- `homeAssistant`: `{ "enabled": false, "discoveryPrefix": "homeassistant" }`.

## Home Assistant discovery

When enabled, retained configs are published to
`<prefix>/<component>/sqmeter_<mac>/<object>/config` on every connect and after settings change;
entities of disabled groups get an empty retained config (removed). One device per SQMeter
(`identifiers: ["sqmeter_<mac>"]`). Entities read `<base>/state` with `value_template`, and are
available only while `<base>/availability` is `online` and their group's `status` is `ok`.

| Group | Entities |
|---|---|
| sky | SQM, NELM, Bortle, Illuminance |
| environment | Temperature, Humidity, Pressure, Dew point |
| clouds | Sky temperature, Cloud cover |
| rain | Raining (binary_sensor, moisture), Rain intensity |
| wind | Wind speed, Wind gust, Wind direction |
| safety | Safe (binary_sensor, device_class safety: `on` = unsafe; state from `<base>/safe`) |
| always | Alerts (switch on `<base>/alerts/armed` / `/set`) |
