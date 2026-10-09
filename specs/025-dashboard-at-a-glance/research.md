# Research: Dashboard at a glance

## D1 Where the inventory lives

**Decision**: `web/src/dashboard/inventory.json`, beside the code that reads it. Shown entries reference translation keys and the Playwright test that covers them; not-shown entries map schema-path globs (`diagnostics.**`, `partitions.**`) to a reason.

**Rationale**: the web UI owns the dashboard; tests and the check both read one file. JSON keeps it machine-readable (FR-001) and diff-friendly.

**Alternatives**: a TypeScript module (typed, but the Python check would have to parse TS); a YAML file under `specs/` (not shipped with the code it governs).

## D2 What "every field" means (FR-002)

**Decision**: the property paths of `status`, `readings`, `safety` and `alerts-armed` in `specs/016-demo-device-emulation/contracts/schemas/`, walked recursively (`$ref`, arrays as `[]`, maps as `*`), plus every `id` in `lib/SettingsDeps/catalogue.json`. A path is mapped when an inventory source or a not-shown glob matches it, or an ancestor is mapped with `**`.

**Rationale**: the contract schemas are already checked against the device and the demo (`tools/contract-check.py`), so a new device field always lands there first. 273 paths today; globs keep the file short.

## D3 The check

**Decision**: `tools/dashboard/check.py` (standard library), run by the quality gate as rule DASH-02 (auto, no baseline) and by the build. It fails on: an unmapped path or dependency id; an inventory source or glob that matches nothing (stale entry); a shown entry without a test that names it (`inventory: <id>` in `web/tests/dashboard.spec.ts`); a label key missing from `en.json`. DASH-01 (the human rule: a change adding user-visible device state updates the inventory) goes in the coding standard and the constitution's quality gates.

## D4 Show/hide logic

**Decision**: one pure function, `glanceItems(input) -> Item[]` in `web/src/dashboard/glance.ts`, from the readings, status, safety, effective settings, alerts-armed document, the stream state (connected, last message) and the browser's last update check. Each item has the inventory id, severity (`problem` / `info`), priority, text, since and fix. The at-a-glance component only renders it. Vitest covers every rule; Playwright covers each shown entry end to end in the demo (FR-003).

**Rationale**: testable without a browser (constitution III); one place decides order and visibility (Edge Cases priority list).

## D5 Wording shared with Settings, the Alpaca page and the flyout (FR-021)

**Decision**: reuse `describeSchedule()` (web/src/components/settings/alertSchedule.ts) for the alert state and `describeClient()` (web/src/lib/alpacaClients.ts) for the imaging app; move them under `web/src/lib/` if a component import would cross a boundary. No new strings for the same state.

## D6 Sensor cards in a fault state (FR-013)

**Decision**: a card is shown when its sensor is **expected**: light and IR always (the core sensors); BME280 once it has been seen since boot or its status is `error`/`stale` (a `missing` BME280 that never answered is "not fitted"); rain, wind and GPS when switched on in Settings. An expected sensor whose status isn't `ok` shows the card in a fault state: the status in words, the age of its last good reading (`status.sensors.<s>.ageMs`), and what it affects ("Cloud cover and the cloud safety rule can't be evaluated").

**Rationale**: the device has no "fitted" flag for I2C sensors; `missing` from boot is the only signal. A sensor that worked and stopped is never hidden.

## D7 Rain hold remaining time (FR-012)

**Decision**: `Rain::clearRemainingMs(latch, now, clearDelayMs)` in lib/RainLogic; `readings.rain.clearInSeconds` (integer, present only while the latch holds after rain stopped). The demo core already computes the same in `pending()`; it switches to the shared function. Schema, REST docs and contract check updated.

**Rationale**: device-side, so MQTT/REST clients get it too; tiny flash cost.

## D8 Freshness (FR-014)

**Decision**: `Disconnected` (stream not connected) > `Updates stopped` (no message for the existing quiet threshold, `useQuiet`) > `Stale` (`readings.dataStale`) > `Live`. Shown first in the at-a-glance line. When disconnected, cards keep the last values greyed with their age (US2-4).

## D9 Settings not in effect (FR-011)

**Decision**: the dashboard fetches `/api/settings/effective` on load and every 60 s (and after a config save via the existing `sqm:config-saved` event if present, otherwise on focus). Entries for alert channels and safety rules count; when every enabled alert channel is inactive it is a problem item ("No alert channel can send: …").

## D10 Update available (FR-018)

**Decision**: the device keeps no update-check result, so the Updates page stores its last result (`{track, latest, checkedAt}`) in `sessionStorage`; the dashboard shows "Update available: vX" from it. The dashboard never calls `/api/updates/check`.

## D11 Demo marker and clock label (FR-022)

**Decision**: `web/src/lib/demoInfo.ts` exposes `demoInfo(): { clockOffsetMs } | null`, set by main.tsx in demo mode from the demo device (no component imports `demo/`, STRUCT-05). The at-a-glance line shows a "Demo" marker linking to the Demo panel and, when the offset exceeds a minute, labels times as the demo device's time.

## D12 Budget (SC-007)

**Decision**: measure the device UI bundle (`npm run build`, gzip of the JS) before and after; the limit is +4 KB gzip. The inventory JSON is not bundled (only ids and keys are used in code).
