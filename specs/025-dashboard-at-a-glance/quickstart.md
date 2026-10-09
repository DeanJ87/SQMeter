# Quickstart: validate the dashboard at a glance

Prerequisites: `cd web && npm ci && npm run build:demo`.

1. Inventory check: `python3 tools/dashboard/check.py` → `OK`. Add a field to `status.schema.json` → it fails naming `status.<field>`.
2. Logic: `npx vitest run src/dashboard` → every visibility rule.
3. End to end: `npx playwright test tests/dashboard.spec.ts` drives the demo into each inventory state (imaging app waiting / watching / stopped checking; alerts paused; sensor not responding; stream stopped; rain held; settings not in effect; clock unset) and checks the item shows, then hides.
4. By eye in the demo (`npm run preview:demo`):
   - Healthy: one line "Live · Safe · Sending alerts".
   - Settings → Alerts → "Only while an imaging app is connected", no app: "Alerts waiting for an imaging app".
   - Demo panel → Imaging app → Connect, then Stop checking: "Imaging app stopped checking - last checked …".
   - `?scenario=fail-ir`: Cloud and IR cards stay, "Not responding".
   - `?scenario=rain`, then stop rain: "Rain held - clears in N min".
5. Budget: `npm run build`, compare the gzip size of `dist/assets/*.js` with main: ≤ +4 KB.
