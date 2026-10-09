# Demo device core changes (spec 021)

`EmulatedDevice` (tools/demo-core/bridge.cpp):

- `setArmed(on, source)`: `source` is `"ui"`, `"rest"` or `"mqtt"`.
- `armedDocument()`: the `/api/alerts/armed` object in [rest.md](rest.md).
- `statusParts()`: adds `alerts` and `alpaca` (same as the device's `/api/status`).
- `alpaca()`: device requests now count as client activity (ClientWatch).
- `restart()`: forgets Alpaca connections and client state, keeps the pause state.
- `saveState()` / `loadState()`: also keep `reason` and `since` of the pause state.
