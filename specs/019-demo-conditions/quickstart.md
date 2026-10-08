# Quickstart: Demo Conditions

## Build and run

```bash
tools/demo-core/build.sh                       # after lib/ or bridge changes (emscripten)
python3 tools/demo-core/source_hash.py --check
cd web && npm run build:demo && npm run preview:demo   # http://localhost:4173/
```

## Checks

| Scenario | Steps | Expected |
|---|---|---|
| Your thresholds (US1, SC-001) | Settings → clear -30, cloudy -20. Panel → air 20, sky -12, humidity 40 | Device reads clear; raise the sky temperature to -5 → cover rises |
| Clear / Overcast (US3) | Same thresholds → **Clear**, then **Overcast** | Says which thresholds it used; device reaches clear, then overcast |
| Unreachable (US3-2) | Safety → cloud rule off → **Cloud just unsafe** | Explains the rule is off, with a link; inputs unchanged |
| Waits (US2) | Rain 2 mm/h, then 0 | "Rain clear delay - …" counts down; verdict clears at 0 |
| Averaging | Darkest tonight, then **Dark sky** | "Sky brightness averages over 90 s - settled in …" |
| Time & place (US4) | North Pole → 31 Dec 23:00 → **Dawn** | No sunrise: explained, clock unchanged |
| Phone (US5) | 375×667, every settings tab | Save tappable; panel scrolls and closes |
| Links | `?scenario=rain`, `cloud`, `night`, `dawn`, `fail-ir` | Unsafe "Rain detected"; unsafe cloud; dark; sun -12..-6 rising; IR card gone |

## Automated

```bash
pio test -e native -f test_alert_logic
cd web && npx vitest run src/demo && npx playwright test tests/demo.spec.ts
```
