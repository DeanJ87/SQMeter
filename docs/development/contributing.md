# Contributing

See [CONTRIBUTING.md](https://github.com/DeanJ87/SQMeter/blob/main/CONTRIBUTING.md) in the repo root for the full guide.

---

## Quick Reference

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
