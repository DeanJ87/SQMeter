# Data model: Dashboard at a glance

## Inventory (`web/src/dashboard/inventory.json`)

```json
{
  "shown": [
    {
      "id": "alerts-state",
      "label": "glance.alerts.sending",
      "sources": ["status.alerts.armed", "status.alerts.mode", "status.alerts.reason", "status.alerts.since", "alerts-armed.**"],
      "requiredBy": "025 FR-008, 021 FR-005",
      "visibility": { "rule": "always" },
      "priority": 3,
      "location": "glance",
      "test": "alerts-state"
    }
  ],
  "notShown": [
    { "match": "status.diagnostics.**", "reason": "Troubleshooting counters", "shownOn": "System" }
  ]
}
```

- **id**: stable, kebab-case; the Playwright test names it (`inventory: <id>`).
- **label**: the main translation key (FR-019).
- **sources**: schema paths (`<schema>.<path>`, schemas `status`, `readings`, `safety`, `alerts-armed`) or dependency ids (`deps.D-12`); `**` matches a subtree.
- **visibility.rule**: `always` | `when` (with `when`: the condition in words) | `onDemand`.
- **priority**: the Edge Cases order (1 = device unreachable … 9 = update available).
- **location**: `glance` | `card:<card id>` | `onDemand`.
- **notShown.match**: path glob or dependency id; **reason** required; **shownOn** where it is instead, if anywhere.

## Glance item (runtime, `glanceItems()`)

| Field | Meaning |
|---|---|
| `id` | inventory id |
| `severity` | `problem` (blocks observing, silences alerts or degrades data) or `info` |
| `priority` | as the inventory |
| `text` | what is wrong / the state, translated |
| `since` | epoch ms or age, when known |
| `consequence` | plain-words effect, for problems |
| `fix` | `{ label, href }` or `{ label, action }` |

Order: priority, then severity. The healthy line is the `info` items for freshness, verdict and alerts joined with " · ".

## Device field (FR-012)

`readings.rain.clearInSeconds`: integer seconds until the rain hold releases; present only while the hold is in effect after rain has stopped (`raining` true, `rainingNow` false).
