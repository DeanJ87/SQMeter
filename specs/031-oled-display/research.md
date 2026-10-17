# Research: OLED display

Grounded in the code as of `feature/travel-allsky` (spec 030 implemented, facdc46 plus the 031/032
spec merges). Paths are repo-relative.

## What exists today

- **No display code.** Nothing in `src/`, `include/` or `lib/` drives a screen; `platformio.ini` has
  no display library (`lib_deps`: Adafruit TSL2591, BME280, MLX90614, Unified Sensor, ArduinoJson,
  PubSubClient, TinyGPSPlus, ESPAsyncWebServer, AsyncTCP; NimBLE in the BLE build).
- **I2C**: `src/main.cpp::setupI2C()` starts `Wire` on `sensor.i2cSDA/i2cSCL` at `sensor.i2cFrequency`
  (default 100 kHz, `lib/ConfigModel/src/ConfigModel.cpp`). Every I2C transaction (TSL2591 ticks,
  BME280, MLX90614) runs on the loop task, so a display on the same bus needs no locking if it is also
  driven from `loop()`. The TSL2591 driver is non-blocking (spec 030 D2): a long display transfer
  in one go would still delay the loop (1 KB at 100 kHz is about 95 ms).
- **Loop cadence**: `loop()` runs every ~10 ms (`delay(10)`); slower sensors every
  `sensor.readIntervalMs` (default 5 s), followed by `webServer->refreshSensorSnapshot()`, which is
  where a new reading becomes visible to every consumer (FR-006 hook point).
- **Night**: `Core::night(snapshot, cfg, epoch)` (`lib/DeviceCore/src/DeviceCore.cpp`) gives
  `known`/`isNight` from a GPS fix, else `location`, and the clock; the limit is
  `alerts.nightSunAltitudeDeg`. Unknown without a clock or a location. `WebServer::computeNight`
  wraps it; the dashboard reads the same values from `/api/status` `sky`.
- **Readings**: `Core::buildReadings` → `Readings::Snapshot` (`lib/Readings/include/Readings.h`) has every
  value a page needs with a `Status` (`Ok`, `Missing`, `Error`, `Stale`) per group: `sky`
  (authoritative, with `source`), `skySensors[2]` (spec 030), `environment`, `infrared`/`clouds`, `gps`
  (`present`), plus `SafetyStatus` (verdict, `unsafeReasons`). Stale data already becomes `Stale`,
  never a stale value (constitution I).
- **Network**: `WiFiManager::isConnected()`, `isInAPMode()`, `getIPAddress()`; hostname and device
  name in `Config::wifi.hostname` / `deviceName`. The setup hotspot name and address come from
  `WiFiManager`/`CaptivePortal` (spec 014).
- **Pins**: `lib/ConfigModel/src/ConfigValidate.cpp` `busPins()` lists the I2C-1, I2C-2, GPS and rain
  pins; `validateWindPins` and `validateI2c2Pins` reject clashes with them. `isValidGpio()`
  (`ConfigInternal.h`) is the classic-ESP32 pin set. `255` already means "unset" (`SensorConfig::PIN_UNSET`).
- **Settings dependencies**: catalogue `lib/SettingsDeps/catalogue.json` (D-01..D-42; reasons such as
  `location-unknown`, `clock-unset`), `Deps::Facts` (detected sensors), mirrored by
  `web/src/lib/settingsDeps.ts`, held together by `test/fixtures/settings-deps/cases.json`.
- **Routes**: `src/WebServerApi.cpp` registers `/api/*` (`requireAuth` on every state change);
  `tools/api/routes.json` lists each route with its auth; `tools/api/operations.json` drives the
  OpenAPI description (`docs/api/openapi.json`, spec 028).
- **Web settings**: tabs in `web/src/components/settings/tabs.ts`; the Device tab
  (`DeviceTab.tsx`) holds Device, Language, Security and Bluetooth cards built from `SettingsCard`,
  `Group`, `DepToggle`, `NumberInput`, `SelectInput`, `ActionButton` (`controls.tsx`).
- **Budgets** (spec 030 `budgets.md`, after 030): esp32dev 1,498,403 B (81.7%), esp32dev-ble
  1,690,667 B (92.1%) of the 1,835,008 B OTA slot. CI warns above 90% and fails above 95%
  (1,743,257 B): **the BLE build has about 52 KB before the hard limit**, which spec 032 also has to
  share. Main config JSON ~2.4 KB default, below 5,100 B with long strings and both sky sensors.
- **Hardware** (SQMeter-Hardware `hardware/kicad/README.md`): the Rev A boards are ESP32-S3; the OLED
  port (J6) is on the main I2C bus (GPIO 8/9 on S3 boards), AUX pins are GPIO 6/7 (39/40 on the ETH
  board). The firmware builds only for the classic `esp32dev` today; S3 board profiles are separate
  work, so every pin this feature adds is configurable with `255` = unset.

## D1 Own minimal SSD1306 driver, no Adafruit_GFX / U8g2

**Decision**: write a small SSD1306 transport (`src/OledDisplay.cpp`, ~150 lines: init sequence,
contrast, display on/off, segment/COM remap for 180°, page-addressed data writes) and do all drawing in
pure C++ into a 1,024-byte framebuffer in a new `lib/DisplayLogic` (5×7 ASCII font for labels, a 10×16
digit font for the large reading, both in flash, ~0.8 KB together).

**Rationale**: flash. The BLE build has ~52 KB to the CI hard limit and spec 032 needs part of it.
Adafruit_SSD1306 + Adafruit_GFX add roughly 15-20 KB with their fonts and a heap framebuffer; U8g2 adds
more with its font tables. Drawing in `lib/` also makes every page natively testable (constitution
III): tests assert exact pixels/text of each page, the no-data marker and the ellipsis rule without
hardware, and the browser demo can run the same renderer.

**Alternatives**: Adafruit_SSD1306 (simplest to write, too large, framebuffer logic untestable
natively); U8g2 (largest, many fonts we would not use); SSD1306Ascii text-only (no large numerals, no
framebuffer for pixel shift).

## D2 Non-blocking transfer: dirty pages, one page per loop pass

**Decision**: the framebuffer is 8 pages × 128 bytes. The renderer marks changed pages dirty; the
transport sends at most one dirty page (one 129-byte I2C write, ~12 ms at 100 kHz, ~3 ms at 400 kHz)
per `loop()` pass. Commands (contrast, on/off) are single short writes. A full redraw completes in
8 passes (~100-200 ms); a value change usually touches 2-3 pages.

**Rationale**: keeps the loop and the TSL2591 cadence unaffected (constitution IV: loop never blocks
on long work), avoids sharing the bus across tasks.

**Alternatives**: a display task (bus would need a mutex shared with every sensor driver); raising the
bus clock (not the display's decision; other sensors may not like 400 kHz on long cables).

## D3 Detection and hot-plug

**Decision**: a cheap probe (`beginTransmission(0x3C)`/`endTransmission()` == 0, then a one-byte status
read that must succeed) at boot and when the display is enabled; while enabled and not detected,
re-probe every 5 s; while detected, a failed data write counts as a fault and after 3 consecutive
failures the display is reported not detected and probing resumes. On re-detection the init sequence
is re-sent and the whole framebuffer marked dirty. A disabled display is never probed or written to
(FR-002, FR-016: zero bus traffic). The probe/back-off timing lives in `lib/DisplayLogic`
(`DisplayLink`, natively tested); the I2C calls in `src/OledDisplay.cpp`.

**Rationale**: FR-001 and the hot-plug edge case. Another device at 0x3C will ACK; the status-read
check rejects devices that NACK reads, and the clash is documented (edge case) rather than solved:
SSD1306 status bits are not a reliable identity across clones.

**Alternatives**: identify by status-register bits (clones and SH1106 differ; would reject working
displays); probe constantly (needless bus traffic).

## D4 Pages, view model and skipping

**Decision**: `Core::displayView(snapshot, readings, safety, cfg, night, network)` in
`lib/DeviceCore` builds a plain `Display::View` (values plus validity per item, already in device
units, plus each page's `available` flag). `lib/DisplayLogic` renders a page from the view. A page is
**available** when its data source is enabled/present: Sky (authoritative sky sensor enabled), Environment
(BME280 detected), Cloud (MLX90614 detected), Safety (always: the verdict always exists), GPS
(`gps.enabled`), Battery (`battery.present`, spec 032; always unavailable until 032 lands), Network
(always). An available page with missing/faulted/stale values shows the no-data marker `--` in place
of each such value (FR-005). The cycle visits only selected **and** available pages in the
configured order; with one visible page it doesn't cycle; with none visible it falls back to Network
(so a user can always find the device).

**Rationale**: one place decides what each page shows, from the same snapshot every interface uses;
no stale value can reach the screen because `Readings` already turned it into `Stale`.

**Alternatives**: rendering directly from `SensorSnapshot` in the firmware (untestable, duplicates
status rules).

## D5 Night behaviour and the unknown-night case

**Decision**: night = `Core::night(...).isNight` (same limit as the dashboard and alerts,
`alerts.nightSunAltitudeDeg`). **When night is unknown (no clock or no location), the display behaves
as at night** (night layout, minimum brightness, the timeout always applies) until both are known.
"Stay on at night" needs a computable night: without a location it is kept as saved but reported
inactive (settings dependency D-44, reason `location-unknown`, linking to Time & location) and does
not keep the display on. Boot is therefore always dim and always times out: the clock is not set yet
at boot.

**Rationale**: SC-002 ("no light after dusk") cannot be guaranteed if unknown is treated as day. The
spec's edge case ("the owner's night-mode choice applies all the time") is read as: the configured
night mode (dim, minimal, timeout enforced) applies at all hours; FR-015 asks for night options to be
disabled without a location, which is what D-44 does. Fail-dark mirrors constitution I's "doubt
resolves to the safe side".

**Alternatives**: treat unknown as day (bright screen at a dark site after a reboot without NTP: the
field case with no network).

## D6 Power state machine (pure logic)

**Decision**: `Display::Controller` in `lib/DisplayLogic`, ticked from `loop()` with `nowMs`, the
config, `night` and inputs (wake request, blank request, button edge, forced off by power saving
from spec 032). States: `Off`, `On(until)`. Wake events: boot (when `wakeOnBoot`), `POST
/api/display/wake`, button press while off. A button press while on advances the page and restarts
the timeout. Timeout `0` = never (day only); at night `never` is replaced by 60 s unless `stayOnAtNight`
(which only counts when night is known, D5). Blank turns it off at once until the next wake. Page cycling uses `cycleSeconds`
(2-60); a page change shifts the content by one pixel in a 4-position pattern (0,0)/(1,0)/(1,1)/(0,1)
(burn-in edge case). Brightness: low/medium/high = contrast 0x10/0x7F/0xFF by day; night = contrast 0x00
with VCOMH deselect at its lowest (0xDB 0x00), the dimmest an SSD1306 goes.

**Rationale**: all of US3/US4 decided natively and tested with a fake clock (constitution III).

## D7 Wake button

**Decision**: `display.buttonPin` (255 = none), `INPUT_PULLUP`, active low, sampled in `loop()`
(10 ms) with a 30 ms debounce in `Display::Button` (pure). Validated with the existing pin-conflict
checks: added to `busPins()`-style checks so it can't share a pin with I2C-1/I2C-2, GPS, rain, wind
(speed/vane) or the battery charger pins (spec 032); must be a valid GPIO from `isValidGpio()`.

**Rationale**: FR-012; the AUX pins on Rev A are free GPIOs. Polling avoids an ISR and its IRAM cost.

## D8 Configuration (additive)

**Decision**: a new `display` object in the main config JSON (see data-model.md and
contracts/config.md): `enabled` false, `pages` `["sky"]`, `cycleSeconds` 5, `brightness`
`"medium"`, `timeoutSeconds` 60 (0 = never), `wakeOnBoot` true, `stayOnAtNight` false, `rotate180`
false, `buttonPin` 255. In C++ `pages` is a fixed `uint8_t[7]` + count (Config is copied often; no
heap). Missing section → defaults → display off (FR-017). About +190 B in the default JSON, +200 B
worst case: well under 5,100 B (pinned by a test).

## D9 Status and routes

**Decision**: `/api/status` gains `display: {enabled, detected, on, page, nightMode}` (page = page id or
null when off). New routes `POST /api/display/wake` and `POST /api/display/blank`, both
`requireAuth`, return `{"ok": true}` (or 409 `{"error": "Display not detected"}` / `"Display is
off in settings"`), and set an atomic request the loop task applies (handlers run on the AsyncTCP
task; constitution IV). Added to `tools/api/routes.json` (auth required), `operations.json`,
`docs/api/openapi.json`, status schema and the demo device.

## D10 Text on the screen

**Decision**: fixed English labels in `lib/DisplayLogic` (`SQM`, `SQM-L`, `NELM`, `Bortle`, `Temp`,
`Hum`, `Dew`, `Sky`, `Cloud`, `Safe`, `UNSAFE`, `GPS`, `Fix`, `No fix`, `Sats`, `Batt`, `Chg`, `WiFi`,
`AP`, `IP`, `--`). Units: the device has no unit settings for temperature or wind (°C, m/s
everywhere in the firmware); `rain.units` is the only unit choice and no page shows rain. So
temperatures are °C (drawn as `C` with a degree glyph) and sky quality `mag`. Text wider than the
line is cut with a trailing `…` glyph (FR-011, edge case). The docs page lists every label.

**Rationale**: FR-011 and the owner's clarification; no translated text or extra fonts on the device.

## D11 Web UI

**Decision**: a **Display** card on the Settings → Device tab (after Bluetooth): detected badge,
enable `DepToggle`, page list (checkboxes with up/down order buttons built from existing controls),
cycle interval, brightness, timeout (select incl. "Never"), wake on boot, stay on at night (with an
amber "uses light at night" note when on, US3 AC3), rotate 180°, wake button pin, and a **Wake**
`ActionButton`. No dashboard card (the display is an output, not a sensor); the dashboard inventory
records `status.display.*` as not shown with that reason (DASH-02). Options that can't apply are
disabled with a reason and link via new settings dependencies:
- D-43 `display.*` (except `enabled`) ← display detected (runtime, reason `display-missing`);
- D-44 `display.stayOnAtNight` ← night computable (reason `location-unknown`, existing);
- D-45 constraint: `display.buttonPin` valid and not used by another bus/pin (messages).
A mockup (phone and desktop) is reviewed by the owner before the card is built (constitution UI rule,
DS rules cited in contracts/ui.md).

## D12 Demo device

**Decision**: the demo reports a detected display; wake/blank change `display.on`; the page id
rotates with the configured pages and cycle. The WASM core compiles `lib/DisplayLogic` too
(`tools/demo-core/build.sh` sources list), so status values come from the same controller.
Rendering the framebuffer in the demo panel is out of scope (UI size).

## D13 Budgets

Expected: flash +6-9 KB (driver, renderer, fonts, view, routes, JSON), static RAM +1.1 KB (framebuffer,
allocated only when the display is enabled: `std::unique_ptr<uint8_t[]>` → 0 B for devices without
one, FR-016), UI gzip +2-3 KB, main config +~200 B, status document +~120 B
(`STATUS_DOC_CAPACITY` checked by `test_status_document_fits`). Measured and recorded in
`specs/031-oled-display/budgets.md`.
