# Contract: demo session state and links

## sessionStorage `sqm.demo.v2`

```ts
interface SavedV2 {
  version: 2;
  device: string;            // core.saveState()
  conditions: Conditions;    // data-model.md
  ramps: Ramp[];
  timeMultiplier: 1 | 10;
  demoMs: number;            // device uptime clock
  clockMs: number;           // device date/time (UTC ms)
  savedAt: number;           // wall ms
}
```

- **v1 (`sqm.demo.v1`) is migrated.** Its device state and clocks are kept, its `scenario` is
  dropped, conditions take the defaults, and the v1 key is removed.
- **Reset demo** removes both keys (FR-018).

## Links

`?scenario=<id>` applies once on load (research R7):

| id | Applies |
|---|---|
| `night` | time preset `darkest` + shortcut `clear` |
| `rain` | shortcut `rain` |
| `cloud` | shortcut `overcast` with a 40 s ramp |
| `clear` | shortcut `clear` |
| `dawn` | time preset `dawn` |
| `fail-light`, `fail-ir`, `fail-environment`, `fail-rain` | the sensor fault |

Unknown ids are ignored.

## `/api/status` `time`

`iso` is the device's local time with a `+hhmm` offset (e.g. `2026-10-08T23:10:00+0100`), as the
firmware writes it. `timezone` is the POSIX string from the settings.
