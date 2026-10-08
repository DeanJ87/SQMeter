# Research: Demo That Behaves Like the Device

## R1 — How the demo runs the device's own rules (FR-004)

- **Decision**: Compile the firmware's portable C++ (`lib/*`, plus the parts of `src/` this plan moves
  into `lib/`) to WebAssembly with Emscripten, and run it in the browser as the demo's "device core".
- **Rationale**: The device's decisions already live in Arduino-free libraries with native tests:
  sky/cloud/dew (`SkyLogic`), the safety verdict and safe delay (`AlpacaLogic` `SafetyEvaluator`,
  `SafeDelayFilter`), the whole Alpaca API behind a `Backend` interface (`AlpacaLogic` `Router`, already
  reused by `tools/alpaca-sim` for ConformU), the alert engine, templates and stacking (`AlertLogic`), the
  readings document and Home Assistant discovery (`Readings`), sun position (`AlertLogic/SunPosition`).
  Compiling them gives identical behaviour by construction; a TypeScript port would drift.
- **Alternatives considered**: (a) Re-implement in TypeScript with shared golden test vectors — two
  implementations to keep equal, exactly the drift FR-010 is about. (b) Keep canned mocks — fails
  FR-002/FR-003 (today's contradictions). (c) Run the code server-side — the demo must stay a static site.

## R2 — What still has to move into `lib/` first

- **Decision**: Extract from `src/` into `lib/` (no behaviour change, native tests):
  1. `lib/ConfigModel`: the `Config` struct, defaults, `toJson`/`fromJson` and `validate` (today in
     `src/Config.cpp` next to NVS code). `src/Config.cpp` keeps load/save to NVS only.
  2. `lib/DeviceCore`: what `src/WebServer.cpp` decides per reading - night/darkness (`computeNight`),
     safety inputs and thresholds from config, the safety status document (`appendSafetyStatus`), alert
     inputs/rules, template variables and application (`alertVars`, `applyAlertTemplate`), the sensor
     snapshot → readings mapping (`buildReadings`), and the Alpaca `Backend` built from a snapshot.
     The status document's shape (`createStatusJson`) moves too, fed a `SystemInfo` struct (heap,
     partitions, WiFi...) the firmware fills from the hardware and the demo fills with plausible values.
- **Rationale**: These are decisions and document shapes, not hardware access. Constitution III already
  requires decision logic in `lib/`; the demo makes it pay off twice.
- **Alternatives considered**: Only compile what's already in `lib/` and re-create the glue in the demo
  — the glue is where the current demo contradicts itself (independent safety and Alpaca answers).

## R3 — WebAssembly toolchain

- **Decision**: Emscripten (pinned version), `embind` bridge in `tools/demo-core/` exposing one
  `EmulatedDevice` class; output (`.mjs` + `.wasm`) committed under `web/src/demo/core/`. CI rebuilds it
  with the pinned Emscripten and fails if the committed output differs (or regenerates it on main).
- **Rationale**: Committing the output keeps `npm test`, `npm run dev:demo` and the docs workflow working
  without Emscripten installed; the freshness check stops it going stale. ArduinoJson is header-only and
  builds unchanged.
- **Alternatives considered**: Building in every web job (slower CI, every contributor needs emsdk);
  WASI + a custom JS ABI (more glue than embind for the same result).

## R4 — Opening API URLs directly (FR-011)

- **Decision**: Ship the demo's `index.html` also as `404.html`. GitHub Pages serves it for any unknown
  path, the app boots, sees a device path (`/api/...`, `/management/...`, `/setup/...`) and renders the
  emulated device's response: JSON pretty-printed (with its HTTP status) for API paths, the device's own
  redirect for setup paths (`/settings?tab=safety`).
- **Rationale**: A static host can't answer arbitrary paths, and the mock layer only intercepts requests
  made by a page it controls - not a first navigation. 404.html works on any static host and at any base
  path. The page's HTTP status stays 404 (host limitation); what the visitor sees is the response.
- **Alternatives considered**: Rewriting the links to `#/...` views (the URLs would no longer match the
  device's); generating static JSON files at build time (not live, not stateful).

## R5 — State and persistence (FR-008)

- **Decision**: The device core holds the live state; the demo saves `{config, armed, history, alerts,
  scenario, clock offset}` to `sessionStorage` on every change and restores it on load. "Reset demo"
  clears it. Storage errors (private mode) fall back to in-memory.
- **Rationale**: Clarified 2026-10-08: lasts until the tab closes.

## R6 — Sensors and weather scenarios (FR-009)

- **Decision**: A small TypeScript sky simulator produces raw sensor inputs (lux, sky/ambient IR
  temperatures, temperature, humidity, pressure, rain rate, wind) from the browser clock, the configured
  location (darkness via the core's sun position) and the active scenario. Everything derived from them
  (SQM, cloud cover, verdict, alerts) comes from the core. Scenarios: rain, clouding over, clearing,
  sensor failure (light / IR / environment / rain), dawn. Long delays use a "demo time" multiplier,
  labelled in the panel.
- **Rationale**: Inputs are the only thing the demo invents; decisions stay the device's.

## R7 — No outbound traffic (FR-006, FR-007)

- **Decision**: Every endpoint that would reach the outside world on a device (update check/apply,
  uploads, alert tests, MQTT test, WiFi scan/connect, restart, NTP) is answered by the demo with the
  device's response shapes and a `demo: true` marker the UI shows as "Demo: nothing was sent". The demo's
  CSP (`connect-src 'self'`) blocks anything else; a Playwright test records all requests while
  exercising every action and asserts they're same-origin static files.
- **Rationale**: Defence in depth: the code doesn't call out, and the browser wouldn't let it.

## R8 — Contract checks (FR-010)

- **Decision**: JSON Schemas for the readings, status, safety, config and Alpaca management documents in
  `specs/016-demo-device-emulation/contracts/schemas/`. Vitest validates every demo response against them;
  `tools/contract-check.py` validates a real device's responses (run in hardware verification). Because
  both sides build the documents with the same `lib/` code, the schemas mostly guard the glue.
- **Rationale**: Catches drift in either direction with one source of truth per document.

## R9 — Bundle and device impact

- **Decision**: The demo core and simulator are imported only when `VITE_DEMO_MODE` is set (dynamic
  import), so the device's LittleFS image doesn't grow. Firmware flash should be unchanged (code moves,
  doesn't grow); measured per Constitution IV.
