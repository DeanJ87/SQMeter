# Data Model: Demo Conditions

## Conditions (demo device, persisted)

| Field | Type | Range / values | Default |
|---|---|---|---|
| `air.temperature` | °C | -40..85 | 11 |
| `air.humidity` | % | 0..100 | 62 |
| `air.pressure` | hPa | 300..1100 | 1013 |
| `ir.sky` | °C | -70..380 | -10 |
| `ir.ambient` | °C | -40..125 | 11.4 |
| `light.mode` | `sun` \| `set` | | `sun` |
| `light.lux` | lux | 0.0001..88000 | 0.0003 (used when `set`) |
| `rain.rate` | mm/h | 0..150 | 0 |
| `rain.lensFault` | bool | | false |
| `wind.speed` | m/s | 0..60 | 3 |
| `wind.gust` | m/s | 0..60 | 5.9 |
| `wind.direction` | ° | 0..359 | 240 |
| `gps.fix` | bool | | true |
| `faults.{light,environment,infrared,rain,wind}` | bool | | false |
| `steady` | bool | turns off natural variation | false |

Derived for display only: **differential** = `ir.sky − ir.ambient` (FR-003). Setting it writes
`ir.sky = ir.ambient + differential`.

Rules:
- Each setter changes exactly one field (FR-002), except the differential (FR-003) and shortcuts.
- Every write is clamped to its range. The setter returns whether it clamped (FR-004).
- When the device's rain sensor is off, `rain.rate` becomes 0 and `rain.lensFault` false.

## Ramp

`{ field, from, to, startMs, durationMs }` on the device uptime clock (demo ms). A ramp is
replaced by a later ramp or a direct set of the same field. It is removed when done.

## Shortcut result

```ts
type ShortcutResult =
  | { ok: true; changes: Partial<Conditions>; used: string; rampMs?: number; clamped?: string }
  | { ok: false; reason: string; link?: { label: string; route: string } };
```

## Time preset / Location preset

- **Time preset**: `{ id, label }`. `resolve(now, location, tz) → { ok: true, at: Date } | { ok: false, reason }`.
- **Location preset**: `{ id, label, latitude, longitude, timezone }`. The time zone is POSIX.
  Applying a preset sends `{location:{set:true,latitude,longitude}, ntp:{timezone}}` through
  `applyConfig`, exactly as saving Settings would.

## Pending (from the core)

See [contracts/core-pending.md](contracts/core-pending.md). Shown alongside the safety
document's `secondsUntilSafe` and the active ramps.

## Saved state v2

See [contracts/demo-state.md](contracts/demo-state.md).
