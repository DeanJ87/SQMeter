# Research: Demo Conditions

## R1 - Where the inputs live

- **Decision**: A `Conditions` object in the demo device (`web/src/demo/conditions.ts`) holds
  the raw readings, faults, the light mode (follow the sun or set) and active ramps. Each tick,
  `toCoreInputs()` turns it into the JSON the core's `tick()` already reads (`light`,
  `environment`, `infrared`, `gps`, `rain`, `wind`).
- **Rationale**: The core input format is unchanged, so the bridge needs no new input fields.
  Inputs persist with the device state.
- **Alternatives**: keep scenarios as overlays on a baseline. Rejected: hides values and breaks
  FR-002 and FR-011.

## R2 - Ranges (FR-004)

- **Decision**: the sensor datasheet ranges.

  | Input | Sensor | Range |
  |---|---|---|
  | Air temperature | BME280 | -40..85 °C |
  | Humidity | BME280 | 0..100 % |
  | Pressure | BME280 | 300..1100 hPa |
  | Sky (object) temperature | MLX90614 | -70..380 °C |
  | Sensor ambient | MLX90614 | -40..125 °C |
  | Illuminance | TSL2591 | 0.0001..88000 lux |
  | Rain rate | RG-15 | 0..150 mm/h |
  | Wind speed and gust | anemometer | 0..60 m/s |
  | Wind direction | vane | 0..359° |

  Out-of-range typed values are clamped, and the input shows a short message saying so.

## R3 - Shortcut margins and formulas (FR-008)

- **Cloud**: the device computes `corrected = (sky − irAmbient) − humidityCorrection/100 × humidity`,
  where humidity is the measured value, or 53% when there's no environment reading. Cover is linear
  from `clearSkyThreshold` (0%) to `cloudyThreshold` (100%).
  - **Clear**: corrected = clear − 3.
  - **Overcast**: corrected = cloudy + 1.
  - **Cloud just unsafe**: cover = limit + 3, capped at 100 (the device is unsafe at cover ≥
    limit). If the cloud rule is off, it's unreachable with a link to Safety.
  - Sky temperature = irAmbient + corrected + correction term. It's clamped to the MLX range, and
    the shortcut says so if clamped.
- **Dark sky (SQM *s*)**: input lux = 10^((12.6 − (s − offset))/2.5), where offset is the
  calibration SQM offset when calibration is on. This is `SkyQuality::luxToSQM` inverted. It
  switches the light to "set".
- **Dew risk**: keep air temperature *T* and set humidity so the dew point is T − (margin − 0.5),
  inverting the device's Magnus formula (`SkyLogic` `dewpointMagnus`, a = 17.27, b = 237.7). A margin ≤ 0.5 °C uses half the
  margin. A margin of 0 is unreachable: the dew point can't exceed the temperature.
- **Rain**: 2.5 mm/h. **Rain stops**: 0. Both need the rain sensor on.
- **Sensor fails**: the fault toggles. US1-5 says faults persist until cleared.
- **Requirements**: every shortcut needs its sensors responding. A faulted sensor makes it
  unreachable, with the reason "IR sensor is set to not responding".

## R4 - Waits (FR-007)

- **Decision**: a new core function `pending()` returns:
  - **Sky averaging**: the window and the seconds until the average reflects the latest step
    change. Night mode only, the same as the driver.
  - **Rain clear delay remaining**: while latched with no rain now.
  - **Alert engine waits**: startup grace, settle and cooldown per condition, from a new
    `AlertEngine::waits(now, rules)` const query in `lib/AlertLogic` (native-tested).

  The safe delay already comes from the safety document's `secondsUntilSafe`, and ramps come from
  the conditions model.
- **Light averaging**: the bridge keeps a time-stamped window of input lux. In night mode the
  device's lux is its mean, emulating the driver's sliding window of 600 ms samples. A step is a
  change of more than 10%, which is larger than the natural variation of ±4%. Changing the window
  resets the samples, as in the driver.
- **Alternatives**: compute waits in TS from settings. Rejected because it could disagree with
  what the device does, e.g. alert cooldown state.

## R5 - Time zones (FR-014, FR-017)

- **Decision**: a small POSIX TZ evaluator (`posixTz.ts`). It supports:
  - `std offset [dst [offset] [,rule[/time],rule[/time]]]`
  - `<…>` quoted names
  - `Mm.w.d` rules
  - times with hours ≥ 24

  This covers every preset and the Settings list. Unparseable strings fall back to UTC, as the
  ESP32 libc does.
- **Use**:
  - local time and date for `core.tick`
  - `/api/status` `time.iso` with a `+hhmm` offset, like the device
  - the panel clock
  - the time presets, which convert local wall time to UTC
- **Location presets**:

  | Place | Latitude | Longitude | Time zone |
  |---|---|---|---|
  | London | 51.5074 | −0.1278 | GMT0BST,M3.5.0/1,M10.5.0 |
  | La Palma | 28.7606 | −17.8816 | WET0WEST,M3.5.0/1,M10.5.0 |
  | Atacama | −24.6272 | −70.4041 | <-04>4<-03>,M9.1.6/24,M4.1.6/24 |
  | Sydney | −33.8688 | 151.2093 | AEST-10AEDT,M10.1.0,M4.1.0/3 |
  | 75° N 1° W | 75 | −1 | UTC0 |
  | North Pole | 90 | 0 | UTC0 |

## R6 - Time presets (FR-013, FR-015)

- **Now**: the browser's current time.
- **Darkest tonight**: `darkestTime`, as today.
- **Dawn**: 45 min before the next sunrise (sun rising through −0.833°).
- **Dusk**: the next sunset (sun setting through −0.833°).
- **Midsummer** and **midwinter midnight**: 00:00 local on 21 June or 21 December of the device
  clock's year, swapped in the southern hemisphere.
- **31 December 23:00**: local time.
- **Unreachable**: dawn and dusk with no crossing in 24 h, and darkest tonight when the sun stays
  above −6°. The panel says why and doesn't move the clock.

## R7 - Scenario links (FR-012)

| `?scenario=` | Applies |
|---|---|
| `night` | darkest-tonight + Clear |
| `rain` | Rain |
| `cloud` | Overcast, 40 s ramp |
| `clear` | Clear |
| `dawn` | Dawn |
| `fail-light`, `fail-ir`, `fail-environment`, `fail-rain` | that sensor's fault |

## R8 - GPS position

- **Decision**: the simulated GPS reports the device's location, which is set by location presets
  or Settings, plus the fix toggle.
- **Rationale**: one place, so Sun & Moon, GPS and darkness can't disagree. Exact coordinates are
  entered in Settings → Time & Location, which the panel links to.

## R9 - Natural variation

- Small sinusoidal variation is applied on top of the inputs: air ±0.3 °C, humidity ±1.5%, sky
  ±0.4 °C, lux ±4%, pressure ±2 hPa, wind ±1.2 m/s, direction ±25°.
- A **Hold steady** switch turns it off.
- The variation is smaller than every shortcut margin. The worst case is sky ±0.4 °C against a
  margin of 1 °C.

## R10 - SC-002 verification

- **Decision**: Playwright sets inputs and checks the device's `clouds.correctedDelta`,
  `coverPercent` and `sky.sqm` against the device formulas. The firmware's native tests already
  cover `lib/` itself.
- **Rationale**: running the native build side by side from Playwright isn't practical. The core
  *is* the same `lib/` code, so comparing against the documented formulas checks the input path.

## R11 - Phone layout (FR-020)

- At ≤ 560 px the open panel is a full-width bottom sheet, at most 75 vh, scrolling inside, with
  the head (title and Close) sticky.
- Groups are `<details>` elements: Sky & clouds open by default, the others closed.
- The Demo toggle keeps the existing save-bar and toast clearance.
