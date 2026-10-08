# Contract: `EmulatedDevice.pending()`

Returns a JSON string with the things the device is still waiting on at the current tick.

```json
{
  "skyAveraging": { "nightMode": true, "windowSeconds": 90, "settlingSeconds": 42 },
  "rainClear": { "latched": true, "rainingNow": false, "remainingSeconds": 680 },
  "alerts": [
    { "condition": "safety", "kind": "cooldown", "remainingSeconds": 212 },
    { "condition": "sky", "kind": "settle", "remainingSeconds": 75 },
    { "condition": "startup", "kind": "grace", "remainingSeconds": 30 }
  ]
}
```

- `skyAveraging.settlingSeconds`:
  - is 0 when the average reflects the latest step change (more than 10%) or when not in night
    mode;
  - otherwise it's the window minus the seconds since the step, or the seconds left to fill the
    window after boot or a window change.
- `rainClear.remainingSeconds` is present only while latched with no rain now. It's the clear
  delay minus the time since the last rain.
- `alerts` comes from `Alerts::AlertEngine::waits(nowSeconds, rules)`. It's an empty array when
  alerts are off. Each entry:
  - `condition`: one of `safety`, `rain`, `lens`, `dew`, `sky`, `sensor:<name>` or `startup`.
  - `kind`: `grace` (startup grace), `settle` (the condition must hold), or `cooldown` (time
    since the last notice for that condition).
  - `remainingSeconds`: ≥ 1.

## `AlertEngine::waits`

```cpp
enum class WaitKind : uint8_t { Grace, Settle, Cooldown };
struct Wait { std::string condition; WaitKind kind; uint32_t remainingSeconds; };
std::vector<Wait> waits(uint32_t nowSeconds, const AlertRules &rules) const;
```

- Pure const query; no state change.
- A tracker contributes only while `pending`, meaning its value differs from the one last
  notified. It contributes its remaining settle time if not yet settled, otherwise its remaining
  cooldown.
- During the startup grace, a single `startup`/`grace` entry is returned.
