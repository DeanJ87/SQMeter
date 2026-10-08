# Contributing

See [CONTRIBUTING.md](https://github.com/DeanJ87/SQMeter/blob/main/CONTRIBUTING.md) in the repo root for the full guide.

---

## Quick Reference

**Coding standard:** see [Coding Standards](coding-standards.md); `python3 tools/quality/check.py` runs what CI checks.

**Firmware build (must be zero warnings):**
```bash
pio run
```

**Unit tests (logic in `lib/`, run on your computer):**
```bash
pio test -e native
cd web && npm test
```

Decision logic lives in `lib/` with a native test suite under `test/`, so it can be tested without hardware:

| Library | What |
|---|---|
| `AlpacaLogic` | Alpaca API router, safety verdict, ObservingConditions mapping |
| `AlertLogic` | Alert engine, delivery priorities, sun position |
| `SkyLogic` | Lux → SQM, NELM, Bortle; cloud model; dew point |
| `RainLogic` | RG-15 line parsing and the rain latch |
| `SafetyHistoryLogic` | The safety history kept across restarts |
| `WindLogic`, `BleLogic` | Wind statistics; Bluetooth payloads |
| `Readings` | The readings document (REST, WebSocket, MQTT) and Home Assistant discovery |
| `ReleaseLogic` | GitHub release list for updates |
| `ConfigModel` | Settings: defaults, JSON, validation |
| `DeviceCore` | Per-reading decisions and documents: readings, safety, darkness, alerts, Alpaca snapshot |
| `SensorTypes` | The sensor reading structs |
| `CaptiveDns` | DNS for the setup hotspot |

**The demo's device core.** The live demo runs the code in `lib/` compiled to WebAssembly (`web/src/demo/core/`, committed). After changing anything in `lib/` or `tools/demo-core/`, rebuild it:

```bash
brew install emscripten          # or the emsdk; version in tools/demo-core/VERSION
tools/demo-core/build.sh
cd web && npm run build:demo && npx playwright test tests/demo.spec.ts
```

CI fails with "The demo's device core is out of date" if you forget. Response formats are checked against `specs/016-demo-device-emulation/contracts/schemas/` - by the demo's tests, and on a device with:

```bash
python3 tools/contract-check.py http://<device>
```

If a device response changes on purpose, capture fresh samples and regenerate the schemas with `tools/contracts/generate_schemas.py` (see its header).

**Web UI dev server:**
```bash
cd web
ESP32_IP=<device-ip> npm run dev
```

**Docs preview:**
```bash
pip install mkdocs-material
mkdocs serve
```

Open a PR from a branch — one feature or fix per PR. Keep commit messages short and imperative.
