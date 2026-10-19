# Contract: readings and status

## `battery` in the readings document (FR-011)

Written by `lib/Readings` for `GET /api/sensors`, `/ws/sensors` and MQTT `<base>/state` (MQTT only when
`mqtt.publish.battery` is on). Always present in REST/WebSocket.

Without a gauge (every existing board), or with monitoring switched off:

```json
"battery": { "present": false }
```

With a gauge:

```json
"battery": {
  "present": true,
  "status": "ok",
  "ageMs": 1200,
  "estimating": false,
  "percent": 82.4,
  "voltage": 3.912,
  "ratePercentPerHour": -6.2,
  "state": "discharging",
  "usbPower": false,
  "fault": null,
  "timeToEmptyMinutes": 797,
  "timeToFullMinutes": null,
  "powerSaving": false
}
```

| Field | Type | Rule |
|---|---|---|
| `status` | `ok` / `missing` / `error` / `stale` | the gauge's status, as every group; not ok → only `present`, `status`, `ageMs`, plus `state` and `usbPower` when the charger pins are set (they come from the charger, e.g. pack removed on USB → `no-battery` with the gauge missing) |
| `estimating` | bool | gauge just reset (research D2) |
| `percent` | number (1 dp) \| null | null while estimating |
| `voltage` | number (3 dp) | V |
| `ratePercentPerHour` | number (1 dp) \| null | positive when charging; null until 60 s of samples |
| `state` | `discharging` / `charging` / `charged` / `fault` / `no-battery` | charger pins, else rate (research D3) |
| `usbPower` | bool \| null | null when the charger pins are not set |
| `fault` | `temperature` \| null | with `state: "fault"` |
| `timeToEmptyMinutes` / `timeToFullMinutes` | integer ≤ 5940 \| null | research D4 |
| `powerSaving` | bool | policy in Saving or ShutdownPending |

`Readings::DOC_CAPACITY` and the 3,072-byte MQTT limit stay satisfied with every group ok, both sky
sensors and the battery (pinned by `test_worst_case_document_fits`). Schema:
`specs/016-demo-device-emulation/contracts/schemas/readings.schema.json` (`battery`, optional object).

## `/api/status`

- `battery`: the same object as above, plus `shutdownInSeconds` (integer, only while shutdown is pending).
- `sensors.battery`: health like the other sensors (`present`, `status`, `ageMs`) — only when a gauge is present.
- `diagnostics.battery` (only with a gauge): `version` (hex string), `vcellRaw`, `socRaw`, `crateRaw`,
  `statusRaw`, `resetSeen`, `charger: {stat1, stat2, pg}` (levels, or null when pins unset),
  `gaugeAlert` (bool \| null), `policy` (`normal`/`saving`/`shutdown`), `currentMa` (number \| null).
- `Deps::Facts`: `batteryDetected` (also in `/api/settings/effective` `facts`).

`Core::STATUS_DOC_CAPACITY` re-pinned by `test_status_document_fits`; status and settings-effective
schemas updated.
