# Contract: display configuration

Part of the main config JSON (`GET/POST /api/config`, NVS main key, 5,100-byte limit). Additive:
a config without `display` loads with the defaults below and the display off (FR-017). Older
firmware ignores the section.

```json
"display": {
  "enabled": false,
  "pages": ["sky"],
  "cycleSeconds": 5,
  "brightness": "medium",
  "timeoutSeconds": 60,
  "wakeOnBoot": true,
  "stayOnAtNight": false,
  "rotate180": false,
  "buttonPin": 255
}
```

| Key | Values | Validation message (exact, device-built, translated via the device catalogue) |
|---|---|---|
| `pages` | 1..7 unique names from `sky`, `environment`, `cloud`, `safety`, `gps`, `battery`, `network`, in display order | `Display: choose 1 to 7 different pages` |
| `cycleSeconds` | 2..60 | `Display: cycle interval must be 2 to 60 seconds` |
| `brightness` | `low`, `medium`, `high` | `Display: brightness must be low, medium or high` |
| `timeoutSeconds` | 0 (never) or 10..1800 | `Display: timeout must be 10 seconds to 30 minutes, or never` |
| `buttonPin` | 255 (none) or a valid GPIO | `Display: wake button pin is not a valid GPIO` |
| `buttonPin` | not an I2C-1/I2C-2, GPS, rain, wind or battery (spec 032) pin | `Display: wake button pin is already in use` |

Unknown page names are rejected (not dropped), so a typo is visible. Partial updates (`POST
/api/config` with only some `display` keys) keep the other keys, as for every section.

## Settings dependencies (catalogue additions)

| ID | Class | Settings | Depends on | Reason code → text → fix |
|---|---|---|---|---|
| D-43 | dependency, runtime | `display.pages`, `display.cycleSeconds`, `display.brightness`, `display.timeoutSeconds`, `display.wakeOnBoot`, `display.stayOnAtNight`, `display.rotate180`, `display.buttonPin` | display detected at 0x3C (`Facts.displayDetected`) | `display-missing` → "No display detected" → `device#display` |
| D-44 | dependency | `display.stayOnAtNight` | night computable (location set or GPS fix, clock set) | `location-unknown` (existing) |
| D-45 | constraint, feature | `display.buttonPin` | valid GPIO not used elsewhere | messages above |

`display.enabled` is in `noDependency` (its own section switch). Reported entries are implemented in
`lib/SettingsDeps` and `web/src/lib/settingsDeps.ts` and held together by
`test/fixtures/settings-deps/cases.json`; `docs/reference/settings-dependencies.md` is regenerated.

## Defaults and fixtures

`test/fixtures/settings-deps/default-config.json`, `test/fixtures/config-ranges.json` (ranges above),
`specs/016-demo-device-emulation/contracts/schemas/config.schema.json` and the web Zod schema
(`web/src/validation/configSchema.ts`) carry the same keys, defaults and ranges.
