# Quickstart: Validating the Demo

## Prerequisites

- Node 20, `cd web && npm ci`
- For rebuilding the device core only: Emscripten (pinned version in `tools/demo-core/VERSION`)

## Run

```bash
cd web && npm run build:demo && npm run preview:demo   # http://localhost:4173/
```

## Scenarios (each maps to the spec)

1. **Links (US1, SC-001)**: open every page; follow every link on the Alpaca page; open
   `/management/v1/description` and `/api/v1/safetymonitor/0/issafe` directly. Expected: the emulated
   response, never the host's 404 page. Automated: `npx playwright test tests/demo-links.spec.ts`.
2. **One device (US1, SC-002)**: trigger **Rain**; dashboard, Alpaca page and Alpaca live state all say
   unsafe with "Rain detected". Automated in `tests/demo-consistency.spec.ts`.
3. **Settings take effect (US2, SC-003)**:
   - Settings → Time & Location → GPS off → Save → dashboard has no GPS card; time source NTP.
   - Enter a location → Save → Sun & Moon and Settings → Alerts darkness use it.
   - Safety → maximum cloud cover below the current reading → Save → unsafe with that reason.
   - Sensors → rain sensor off → rain gone from dashboard, readings and Alpaca.
   - A restart-required change asks for a restart and applies after it.
4. **Nothing leaves the browser (US3, SC-004)**: `npx playwright test tests/demo-network.spec.ts`
   exercises send tests, update check/install, uploads, WiFi setup, restart; asserts all requests are
   same-origin static files.
5. **Contracts (US4, SC-005)**: `npm test` validates every emulated document against
   `specs/016-demo-device-emulation/contracts/schemas/`. Against a real device:
   `python3 tools/contract-check.py http://<device>`.
6. **Scenarios (US5, SC-006)**: the demo panel's Rain / Cloud / Clear / Sensor failure / Dawn each change
   the verdict, reasons, alerts list and bell as listed in `data-model.md`.
7. **Persistence (FR-008)**: change a setting, refresh → kept; close the tab and reopen → defaults;
   **Reset demo** → defaults.
8. **Device unchanged**: `pio test -e native`, both firmware builds, ConformU against `tools/alpaca-sim`
   and a device; device status/readings pass `tools/contract-check.py`.
