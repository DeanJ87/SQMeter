# Research: Rain needs cloud

## D1. Where the rule lives

- **Decision**: a new pure unit `lib/AlpacaLogic/RainTrust` with `classifyRain()` (stateless) and a
  `RainTrustLatch` (one bool of state). The caller updates the latch once per safety pass and
  passes the result to `evaluateSafety` in `SafetyInputs::rain`.
- **Rationale**: Principle III and STRUCT-02/07: the firmware, the demo core and the native tests
  run the same code. Keeping `evaluateSafety` stateless keeps every existing test and the Alpaca
  simulator unchanged; a `SafetyInputs` without a classification defaults to "trusted", which is
  today's behaviour.
- **Alternatives**: state inside `evaluateSafety` (breaks its purity and the simulator); a
  TypeScript copy for the demo (forbidden by STRUCT-07).

## D2. What "the sky sensor is usable" means

- **Decision**: usable = the MLX90614 initialised, status OK, read at least once, its last read
  within the same stale limit the verdict uses, and the cloud model produced a finite cover with a
  known condition. Built in `Core::safetyInputs` from the snapshot.
- **Rationale**: FR-006 and the edge case "healthy but cloud cover can't be computed". It is the
  same freshness rule the readings document and Alpaca use for the IR sensor.
- **Alternatives**: the readings group status only (doesn't cover an uncomputable model).

## D3. The latch

- **Decision**: once rain is trusted (for any reason, including "setting off" and "no usable sky
  reading"), it stays trusted while `raining` (the RG-15's instantaneous rain or its
  `rainClearDelayMs` hold-off) stays true. It resets only when `raining` is false.
- **Rationale**: FR-007 and the flicker edge case. Trust from an untrusted sky also latches, so a
  sky sensor recovering mid-shower with a clear reading can't flip the verdict to safe.
- **Alternatives**: latching only cloud-confirmed rain (a stale sky sensor coming back clear would
  release the rain).

## D4. Reason wording (Principle I, FR-008)

- **Decision**:
  - setting off: `Rain %.2f mm/h`, or `Rain detected` below 0.01 mm/h (the hold-off and the first
    drops report no rate);
  - cloud at or above the level: `Rain %.2f mm/h with cloud %.0f%% >= %.0f%%`;
  - no usable sky reading: `Rain %.2f mm/h with no cloud reading`;
  - latched: `Rain %.2f mm/h, held until dry`.
- **Rationale**: every reason states the measured values; `>=` matches the other reasons, which the
  device message catalogue already translates. The literal format strings go straight to
  `addReasonf`, so `tools/i18n/gen_device_catalog.py` finds them.

## D5. Reporting the flag

- **Decision**: the safety document gains `dewSuspected` (always) and `dew` (only while suspected:
  `rainIntensity` mm/h, `cloudCoverPercent`, `sinceAgeMs`). MQTT `<base>/safety` carries the same
  document; Home Assistant gets `binary_sensor.<id>_dew_suspected` (device class `problem`) reading
  `value_json.dewSuspected` from `<base>/safety`, discovered when the rain sensor and safety
  publishing are on. The dashboard reads `safety.dewSuspected` from the readings stream.
- **Rationale**: one document for REST, WebSocket and MQTT (as spec 035 did for holds); no new
  topic.

## D6. Alert

- **Decision**: a new event `dew_suspected` (default off), `AlertType::DewSuspected`, raised when
  the flag has held for `alerts.sensorFaultDelaySeconds` (the lens-fault precedent), once per
  episode, cooldown as usual. Title "Rain sensor: dew suspected". It is stored with the battery
  events' NVS key, which has room.
- **Rationale**: FR-010; alerts read the flag from the safety status, so they can't change it.

## D7. Settings dependency

- **Decision**: D-52, `alpaca.rainNeedsCloud`, unmet "inactive", reasons `rain-off` (rain sensor
  off) and `infrared-missing` (IR sky sensor not detected; rain is then always trusted). The
  `alerts.events.dew_suspected.level` row joins D-04 (alerts off) and D-05 (rain sensor on).
- **Rationale**: FR-012 and Principle V; reuses existing reasons and fix links.

## D8. Config

- **Decision**: `alpaca.rainNeedsCloud` (bool, default false) and `alpaca.rainNeedsCloudPercent`
  (whole number 1-100, default 10). Absent keys keep the defaults, so older configs load with the
  rule off (FR-014). Backup (spec 033) and `/api/config` carry the whole `alpaca` object already.
