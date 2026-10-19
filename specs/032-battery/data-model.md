# Data model: Li-ion battery and fuel gauge

Pure types live in `lib/BatteryLogic` (no Arduino headers) unless noted; TypeScript mirrors in `web/src/types`.

## BatteryConfig (`lib/ConfigModel/include/Config.h`, `Config::battery`)

| Field | Type | Default | Rule |
|---|---|---|---|
| `enabled` | bool | true | monitoring; acts only with a gauge (FR-005) |
| `lowPercent` | uint8 | 20 | 5..50 (FR-006) |
| `criticalPercent` | uint8 | 10 | 2..25, < `lowPercent` |
| `shutdownPercent` | uint8 | 5 | 2..10, < `criticalPercent` (FR-010) |
| `capacityMah` | uint16 | 0 | 0 (unset) or 500..50000 |
| `powerSaving` | bool | true | FR-010 |
| `saveReadIntervalSeconds` | uint16 | 30 | 10..300 |
| `shutdownGraceSeconds` | uint16 | 30 | 10..300 |
| `alertPin`, `stat1Pin`, `stat2Pin`, `pgPin` | uint8 | 255 | 255 or valid GPIO; distinct; not used by another bus, wind or the display button; `pgPin` RTC-capable; STAT1/STAT2/PG set together or not at all |

`AlpacaConfig` adds `batteryEnabled` (false), `batteryMinPercent` (10, 2..50), `batteryAlert` (true),
`batteryPhoneAlarm` (true). `MQTTConfig::Publish` adds `battery` (true). `AlertsConfig` adds six
`EventSetting`s (`batteryLow` 2, `batteryCritical` 4, `batteryShutdown` 4, `powerRestored` 0,
`chargeComplete` 0, `chargeFault` 2), persisted as `AlertsPart::Battery` under NVS key `alertsBatt`.

## Battery::GaugeSample / GaugeDecoder

Raw: `vcell`, `soc`, `crate`, `status`, `version` (uint16 registers). Decoded: `voltage` (V), `percent`
(0..100, clamped), `rateRaw` (%/h), `resetSeen` (RI). `GaugeDecoder` tracks the estimating window
(2 s after reset/first detection; SOC 0 with VCELL > 3.5 V) and the 10-minute EMA of the rate (valid
after 60 s).

## Battery::ChargerDecoder

Input: STAT1, STAT2, PG levels (or "pins unset") each tick with `nowMs`. Output: `ChargeState`
(`Discharging`, `Charging`, `Charged`, `Fault`, `NoBattery`, `Unknown`), `usbPower` (optional bool),
`fault` (`None`, `Temperature`), with the 2 s stability and 6 s toggling rules (research D3). With pins
unset, state comes from the smoothed rate.

## Battery::Estimate

`timeToEmptyMinutes`, `timeToFullMinutes` (optional, capped 5,940), `currentMa` (optional, needs
capacity) from percent, smoothed rate, state and capacity (research D4).

## Battery::PowerPolicy

| Field | Meaning |
|---|---|
| `state` | `Normal`, `Saving`, `ShutdownPending` |
| `belowShutdownCount` | consecutive valid reads ≤ shutdown (2 needed) |
| `pendingSinceMs` | start of the grace |
| events out | `enteredSaving`, `shutdownStarted`, `powerRestored`, `sleepNow` (one-shot per tick) |

Inputs: valid percent (or none), charge state, usbPower, config, `dispatcherIdle`, `nowMs`. Transitions
in research D7. RTC (firmware): `sleptForBattery`, `wakeCause`.

## BatterySnapshot (`lib/DeviceCore`, part of `SensorSnapshot`)

`present`, `initialized`, `lastUpdate`, decoded gauge values, `estimating`, charger output, estimate,
policy state, `gaugeAlert` (ALRT level, diagnostics only), raw registers for diagnostics.

## Readings::Battery (`lib/Readings`)

`present`, `status`, `ageMs`, `percent?`, `voltage?`, `ratePercentPerHour?`, `state`, `usbPower?`,
`fault?`, `timeToEmptyMinutes?`, `timeToFullMinutes?`, `powerSaving`, `estimating`
([contracts/readings.md](contracts/readings.md)).

## Safety (`lib/AlpacaLogic`)

`SafetyThresholds`: `batteryEnabled`, `batteryMinPercent`. `SafetyInputs`: `batteryPresent`,
`batteryValid` (present, ok, not estimating, fresh), `batteryPercent`, `shutdownPending`,
`shutdownPercent`. Flags: `UnsafeBatteryLow` (1<<14), `UnsafeBatteryData` (1<<15), `UnsafeShutdown`
(1<<16).

## Alerts (`lib/AlertLogic`)

`AlertType` adds `BatteryLow`, `BatteryCritical`, `BatteryShutdown`, `PowerRestored`, `ChargeComplete`,
`ChargeFault`, `BatteryRule` (`ALERT_TYPE_COUNT` follows). `AlertInputs` adds `battery {present,
valid, percent, state, shutdownStarted, powerRestored}`; `AlertRules` adds `batteryLowPercent`,
`batteryCriticalPercent`, hysteresis 5, per-event enables. Trackers: low, critical, chargeComplete,
chargeFault; shutdown and power restored are one-shot and skip the startup grace.

## Settings dependencies

`Deps::Facts.batteryDetected`; reason `battery-missing`; entries D-46..D-51
([contracts/config.md](contracts/config.md)).
