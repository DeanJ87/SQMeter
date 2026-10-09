# Research: Alert schedule wording and "imaging app lost" alerts

Decisions made while planning (no open questions were put to the user).

## D1 Where the logic lives

- **Decision**: `ClientWatch` in `lib/AlpacaLogic`; `AlertSchedule` in `lib/AlertLogic`; the three
  client events in `AlertEngine`; settings/vars/documents in `lib/DeviceCore`.
- **Rationale**: FR-007/FR-015 put client tracking in the shared Alpaca logic and the rules in
  host-testable code that the demo runs. The firmware and the demo bridge each had their own
  arming code; both now call `AlertSchedule`.
- **Alternatives**: keep arming in `WebServer.cpp` (untestable, duplicated in the bridge).

## D2 How the Router reports activity

- **Decision**: `Router` keeps clock-free per-device counters (`requests`, `disconnects`) and the
  last `ClientID`; `ClientWatch::update(nowMs)` compares them with what it saw last pass.
- **Rationale**: the Router runs on the AsyncTCP task and in `tools/alpaca-sim` (ConformU); a
  clock in the Router would change its `Backend` interface for every user. Counters are plain
  `uint32_t` writes, safe to read from the loop task (as `connected[]` already is).
- **Alternatives**: a `nowMs()` on `Backend` (touches every backend, including the ConformU sim).

## D3 What "watching" means after a clean disconnect

- **Decision**: after a restart, any device request starts watching (spec edge case). After a
  clean disconnect, watching resumes only on a new connect (`connected=true` / `connect`).
- **Rationale**: tools that poll `description` or `name` without connecting after a session ends
  would otherwise trigger "stopped checking" alerts for a session that ended normally (SC-002).

## D4 Event rules

- `client_lost` / `client_back` use one tracker per device with the engine's usual `sync()`:
  cooldown per event, a change held back by the cooldown is sent when it ends, flapping inside the
  cooldown produces at most one notification.
- `client_back` is not sent in the same pass as a clean disconnect (the disconnect request is
  activity, but "is back" next to "disconnected" is noise); the tracker resets silently.
- `client_disconnected` is a one-shot per device, held while its cooldown runs.
- Client events are exempt from the startup grace and from the "only when dark" limits (spec
  edge cases); FR-013 (no session since restart, no alert) is what protects restarts.
- Alpaca disabled in settings: the watch resets, nothing fires.

## D5 Persisted size (FR-022)

- **Decision**: the three client event settings and the two silence times are persisted under a
  second NVS key (`alertclient`), spliced into the document on load. The `/api/config` shape is
  unchanged: everything is still under `alerts`.
- **Rationale**: with today's 9 events at their maximum template length the alerts JSON is
  already close to the 3900-byte limit; 3 more full events would exceed it. A separate key keeps
  every existing configuration valid, and older firmware simply ignores the extra key.
- **Validation**: each persisted part is checked against 3900 bytes before saving.

## D6 Mode setting and migration

- `alerts.sendMode`: `"any"` | `"whileConnected"`. On input `sendMode` wins; a document with only
  `armWithAlpaca` (older UI, scripts) maps `true → whileConnected`, `false → any`.
- `armWithAlpaca` is always written, equal to `sendMode == whileConnected` (FR-020).

## D7 Who paused or resumed

- REST `POST /api/alerts/arm|disarm` accepts an optional `source=ui`; the web UI sends it.
  Without it the source is `rest`. MQTT `<topic>/alerts/armed/set` is `mqtt`.
- **Rationale**: the UI and scripts share the endpoint; an optional parameter keeps scripts
  unchanged (FR-021).

## D8 Mode changes

- `any → whileConnected` with no client connected: paused, reason `waiting-for-client`; with a
  client connected: sending, reason `client-connected`.
- `whileConnected → any`: a pause caused by the mode (`client-disconnected`,
  `waiting-for-client`) ends; a pause by the user stays.
- In `whileConnected`, a connect (no device connected → one connected) resumes and a clean
  disconnect of the last device pauses; silence never changes the connection flag, so it never
  pauses (FR-004).

## D9 Persisting the pause state

- `sqm-alerts` NVS namespace: `armed` (as today), plus `reason` (u8) and `since` (epoch, 0 if
  unknown). A missing `reason` with `armed=false` is the upgrade case → `migrated`
  ("Paused (before the update)"); with `armed=true` → sending.
- The demo saves the same three values in its session state.

## D10 Documents

- `/api/alerts/armed`: `{armed, armWithAlpaca, mode, reason, since, sinceAgeMs}` (`since` ISO UTC or
  `null` without a clock; `sinceAgeMs` from the uptime clock, `null` before the first change).
- `/api/status`: `alerts` (the same object) and `alpaca.clients.safetymonitor|observingconditions`
  `{connected, watching, silent, lastCheckedAgeMs, clientId}` (`null` where unknown).

## D11 Template variable names

- **Decision**: `{device}` (as specified), `{silent_for}`, `{last_checked}`, `{client_id}`.
- **Deviation**: the spec wrote `{silentFor}`, `{lastChecked}`, `{clientId}`. Every existing
  template variable is snake_case (`rain_rate`, `dew_margin`, `sun_alt`), so the new ones follow
  that; the spec's Wording table is updated to match.
- `{device}` already means the device name for every other event; for client events the event's
  own value wins (the existing rule), so it reads "safety monitor" / "weather device".
- `{last_checked}`: local time `HH:MM` when the clock is valid (worked back from the local time and
  the age), otherwise "N min ago".

## D12 "When to send" control

- A `SelectInput` in a `Field` labelled **When to send**, with the two options from the Wording
  table. There is no radio-group building block in the UI and the constitution forbids new
  control styles; a select with two clearly labelled options meets FR-016/FR-017.

## D13 Resume notification

- The quiet notification on resume keeps its event (`alerts_on`) and becomes "Alerts resumed".

## D14 Demo imaging app

- `web/src/demo/DemoImagingApp.tsx`: connect / go silent / resume checking / disconnect, polling
  the emulated device's Alpaca API through `demoDevice.alpaca()` every 3 s (SafetyMonitor
  `issafe`) and 60 s (ObservingConditions `cloudcover`), measured in demo time so 10× speeds it up.
  `DemoPanel.tsx` gains one line to host it, so spec 019's rebuilt panel can host it too.

## D15 The web UI isn't an imaging app

- **Decision**: device requests carrying `source=ui` are not counted by the Router. The Alpaca
  page's live `devicestate` polling adds it.
- **Rationale**: found while implementing - the Alpaca page polls the device's own Alpaca API
  every 5 s, so leaving the page would raise "The imaging app stops checking" two minutes later.
  The Alpaca spec lets clients send extra parameters; ConformU never sends `source`.

## D16 A user's pause survives a mode change

- Switching to "Only while an imaging app is connected" while paused by the user (UI, REST, MQTT)
  keeps that pause instead of replacing it with "waiting for an imaging app"; switching back to
  "Any time" then still leaves it paused. Only pauses the mode itself caused end with the mode.

## D17 Known limitation: downgrade then upgrade

- Older firmware saves only the main alerts key. After a downgrade, a change of
  `armWithAlpaca` there, and an upgrade again, the stale `alertclient` key's `sendMode` wins.
  Rare, harmless (the user sees the mode in Settings) and not worth a version stamp.

