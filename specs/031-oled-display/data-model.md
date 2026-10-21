# Data model: OLED display

Types are C++ unless noted; pure types live in `lib/DisplayLogic` (no Arduino headers) and are
mirrored in TypeScript where the web UI needs them.

## DisplayConfig (`lib/ConfigModel/include/Config.h`, `Config::display`)

| Field | Type | Default | Rule |
|---|---|---|---|
| `enabled` | bool | false | FR-002 |
| `pages[7]` + `pageCount` | `Display::Page` (uint8) | `{Sky}`, 1 | 1..7 entries, each a known page, no duplicates (FR-004) |
| `cycleSeconds` | uint8 | 5 | 2..60 |
| `brightness` | `Display::Brightness` | `Medium` | `low` / `medium` / `high` (FR-007) |
| `timeoutSeconds` | uint16 | 60 | 0 (never) or 10..1800 (FR-008) |
| `wakeOnBoot` | bool | true | FR-008 |
| `stayOnAtNight` | bool | false | FR-009; only counts when night is known (research D5) |
| `rotate180` | bool | false | FR-010 |
| `buttonPin` | uint8 | 255 (unset) | 255, or a valid GPIO not used by I2C-1/I2C-2, GPS, rain, wind or battery pins (FR-012) |

JSON names and messages: [contracts/config.md](contracts/config.md). Missing section → defaults →
display off (FR-017).

## Display::Page (`lib/DisplayLogic/include/DisplayTypes.h`)

`enum class Page : uint8_t { Sky, Environment, Cloud, Safety, Gps, Battery, Network }`,
`PAGE_COUNT = 7`; `pageName()` (`"sky"`, `"environment"`, `"cloud"`, `"safety"`, `"gps"`,
`"battery"`, `"network"`) and `parsePage()` (exact names only).

## Display::View (`lib/DisplayLogic`, built by `Core::displayView` in `lib/DeviceCore`)

What the screen may show, already decided (status rules from `Readings`, no stale values):

| Group | Fields | Available when |
|---|---|---|
| sky | `valid`, `sqm`, `label` (`"SQM"`/`"SQM-L"`), `bortle`, `nelm`; `other.present`, `other.valid`, `other.sqm`, `other.label` | authoritative sky sensor enabled |
| environment | `valid`, `temperatureC`, `humidity`, `dewpointC` | BME280 detected |
| cloud | `valid`, `skyTemperatureC`, `condition` (`clear`/`cloudy`/`overcast`) | MLX90614 detected |
| safety | `known`, `safe`, `firstReason` (string, cut to fit) | always |
| gps | `valid`, `fix`, `satellites`, `latitude`, `longitude` | `gps.enabled` |
| battery | `valid`, `percent`, `charging` state text | `battery.present` (spec 032; false until then) |
| network | `name` (device name), `mode` (`wifi`/`ap`/`none`), `ip`, `apName`, `connected` | always |

Each group has `available` (page can be shown) and per-value validity (shows `--` when invalid).

## Display::Controller (`lib/DisplayLogic`)

Inputs per tick: `nowMs`, `DisplayConfig`, `night {known, isNight}`, `View` availability, events
(`wakeRequested`, `blankRequested`, `buttonPressed`, `forcedOff` from spec 032 power saving),
`detected`.

State:

| Field | Meaning |
|---|---|
| `on` | lit |
| `offAtMs` / `forever` | when the timeout ends; `forever` only by day with timeout 0, or at known night with `stayOnAtNight` |
| `pageIndex` | index into the visible page list |
| `pageSinceMs` | for cycling |
| `shift` | 0..3 pixel-shift position (advances on page change) |
| `nightMode` | `!night.known || night.isNight` |

Transitions:

- `Off` → `On`: boot with `wakeOnBoot`, wake request, button press (while enabled, detected and not forced off).
- `On` → `Off`: timeout reached; blank request; display disabled; not detected; forced off.
- `On` + button: next visible page, timeout restarts.
- `On` + `cycleSeconds` elapsed and >1 visible page: next visible page.
- Visible pages = configured order ∩ available; empty → `{Network}`.
- Effective timeout: `timeoutSeconds`, except `0` at night (or unknown night) → 60 s unless `stayOnAtNight` with night known.

Outputs: `on`, `page` (or none), `nightMode`, `contrast` (day: low 0x10 / medium 0x7F / high 0xFF; night 0x00),
`shift`, `rotate180`.

## Display::Frame (`lib/DisplayLogic`)

128 × 64 monochrome framebuffer, 8 pages × 128 bytes (SSD1306 page layout), with a dirty bit per page.
`render(Page, View, nightMode, shift)` clears and draws; a page whose bytes didn't change stays clean.
Night layout: the page's single primary value in the large font, no inverse blocks or borders.

## Display::Link (`lib/DisplayLogic`)

Probe and fault timing: `detected`, consecutive write failures (3 → not detected), next probe time
(every 5 s while enabled and not detected), `needsInit` after (re)detection. The firmware supplies
probe/write results.

## Display::Button (`lib/DisplayLogic`)

Debounce (30 ms) of a raw active-low level sampled every loop; emits one `pressed` per press.

## Status (`/api/status` `display`)

`enabled`, `detected`, `on`, `page` (page name or null), `nightMode`. See
[contracts/status-and-routes.md](contracts/status-and-routes.md).

## Settings dependencies

`Deps::Facts.displayDetected` (new); entries D-43..D-45 ([contracts/config.md](contracts/config.md)).
