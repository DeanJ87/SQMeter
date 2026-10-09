# Research: Settings Dependencies

All decisions were made without an interactive session (the builder runs unattended); each records the alternatives considered.

## R1 - Where "is this setting in effect?" is decided

- **Decision**: One pure evaluator in a new library `lib/SettingsDeps` (`Deps::evaluate(config, facts)`), run by the firmware, the native tests and the browser demo (WASM). The web UI has a TypeScript mirror (`web/src/lib/settingsDeps.ts`) used only to *preview* unsaved changes; whenever the device's report is loaded and the form is clean, the UI shows the device's report as-is.
- **Rationale**: Constitution III (decision logic in `lib/`, native-tested) and FR-011 (the demo uses the device's own logic). The UI can't run the WASM core on a real device (LittleFS budget), and FR-006 needs a preview of a draft that the device hasn't seen.
- **Parity**: both implementations run the same fixture file, `test/fixtures/settings-deps/cases.json` (config overlay + facts → expected state, reason code and text per setting). The native suite and Vitest each assert every case, so a rule changed on one side only fails a test.
- **Alternatives**: a declarative JSON rule language interpreted on both sides (more flash, harder to read, still needs two interpreters); computing everything in the UI (contradicts FR-003/US3 - the device must report it).

## R2 - The effective-state report

- **Decision**: `GET /api/settings/effective` returns `{ facts, settings[] }`. Each setting entry: `id` (catalogue ID of the deciding rule), `setting` (config JSON path), `state` (`off` | `active` | `inactive`), and for inactive entries `reason` (code), `text`, `fix` (`tab#anchor`), plus `unmet` (`inactive` | `fail-safe`) for safety rules. `facts` are the runtime facts the device used (WiFi, broker, clock, GPS fix, Bluetooth, sensor detection, what's running since boot).
- **Rationale**: Additive (existing clients unaffected). Returning the facts lets the UI preview a draft with *the device's* facts, so the preview and the post-save report agree. A separate endpoint keeps `/api/status` (pushed every 2 s over WebSocket) small.
- **Auth**: read-only, reveals no secrets - no `requireAuth`, like `/api/status` and `/api/safety`.
- **Alternatives**: a field in `/api/status` (bigger WebSocket frames for every client); per-setting flags inside `/api/config` (mixes stored and derived state; breaks "settings round-trip unchanged").

## R3 - The catalogue as a checked artifact

- **Decision**: The machine-readable catalogue is `lib/SettingsDeps/catalogue.json` (ID, settings, requirement chain, class, unmet behaviour, reason code/text, fix target). `tools/settings-deps/check.py` (stdlib Python, run in CI) verifies:
  1. every reported catalogue ID is implemented in `lib/SettingsDeps` and in `web/src/lib/settingsDeps.ts`;
  2. every catalogue ID is named in at least one native test and one web test (FR-013);
  3. every reported ID has fixture cases, met and unmet;
  4. device code that gates one setting's effect on another setting's switch carries a `dep: D-NN` marker naming a catalogue ID (FR-014 - "undeclared dependency" scan, heuristic: a condition reading switches from two different config sections);
  5. `docs/reference/settings-dependencies.md` matches the catalogue (`--write-docs` regenerates it).
- **Rationale**: FR-012/013/014 and US4 need something CI can fail on and convergence can read. A JSON file is diffable and readable by both the checker and the docs generator.
- **Alternatives**: parse the spec's markdown table (specs are historical once merged - constitution); clang-based analysis of reads (heavy toolchain for a heuristic).

## R4 - Safety rules when their sensor is off (FR-009)

Confirmed from `lib/AlpacaLogic/src/SafetyEvaluator.cpp` and `lib/DeviceCore` (current behaviour, unchanged):

| Entry | Rule | Unmet behaviour | Evidence |
|---|---|---|---|
| D-15 | Unsafe while raining; Unsafe if the rain sensor fails | **inactive** | Both are inside `if (in.rainSensorEnabled)`; with the sensor off they're skipped. |
| D-16 | Max wind speed; Max gust | **fail-safe** | "Wind limit set but the anemometer is disabled or not reporting". |
| D-17 | Max cloud cover | **fail-safe** | An MLX90614 fault is a required-sensor fault: "Sensor fault: MLX90614 IR". |
| D-18 | Min sky darkness | **fail-safe** | A TSL2591 fault is a required-sensor fault. |
| D-19 | Max humidity; Min dew-point margin | **fail-safe** | "Humidity sensor fault - humidity/dew point rules can't be evaluated". |

- **Decision**: record these as-is. The safety document gains `rulesNotInEffect` (additive) listing inactive rules, e.g. "Unsafe while raining - rain sensor is off", so FR-009's "the verdict lists it" holds. The UI says "Not in effect" for inactive rules and "Reports unsafe while on" for fail-safe ones.
- **Note**: the UI used to say "Reports unsafe while on" for the rain rules, which was wrong (they're ignored). Fixed.

## R5 - D-32 ArduinoOTA without a password: Dependency, not Constraint

- **Decision**: Dependency. A saved `ota.enabled` with an empty password stays saved and is reported inactive ("Set an upload password"). The toggle is not blocked, because the password field only appears once it's on (the fix is inline), like the phone alarm's passkey (D-31).
- **Rationale**: Making it a Constraint would make `Config::validate` reject configs that devices in the field already store, which would fail to load (constitution: config from an older release MUST load). The firmware already ignores it at run time.

## R6 - D-08 "Wake me" without Bluetooth

- **Decision**: The level stays selectable. "Wake me" also means Pushover emergency priority and ntfy max priority, which work without Bluetooth; only the phone ringing depends on Bluetooth. The dependent "setting" is "Wake-me events ring paired phones" (`alerts.wakePhones`, on while any event is at level 4); when its chain is unmet the event table shows "Phones won't ring - <first unmet link>". Blocking the level would remove a working Pushover feature.
- **Finding**: paired phones ring for Wake-level events even while push alerts ("Send alerts") are off - `WebServer::processAlerts` raises the Bluetooth alarm outside the `alerts.enabled` check. So an event at Wake me, the arming options and the night-only options stay *active* with alerts off when phones can ring; push channels always need alerts on.

## R7 - D-14 publish groups default on while their sensors default off (SC-005)

- **Decision**: Keep the defaults (changing defaults is out of scope) and show the inactive state of publish groups neutrally (muted note, not a warning). Nothing is published for a switched-off sensor, so the inactive state is harmless.

## R8 - Alert channels: inactive means "skipped with the reason", never "failed"

- **Decision**: The firmware computes the active channel mask from the evaluator. Channels that are switched on but inactive are recorded on each alert as `skipped` with the reason (e.g. "MQTT is off"), and no delivery is attempted. A test sent to an inactive channel is accepted and answers `skipped` with the reason (spec edge case "Test sends"); the UI doesn't offer "Send test" on an inactive channel.
- **Rationale**: AC US1-1 ("no MQTT alert delivery is attempted or recorded as failed") and FR-002. Recording "skipped" keeps the history honest about what was switched on.

## R9 - D-13 Home Assistant

- **Decision**: Discovery (`mqtt.homeAssistant.enabled`) needs MQTT on. The "Alerts" switch entity needs alerts on: while alerts are off, discovery no longer announces the switch (and removes it if it was announced), and the report says "Alerts are off".

## R10 - D-12 alerts follow N.I.N.A. (armWithAlpaca) - shared with spec 021

- **Decision**: Implemented generically as an evaluator rule (`alerts.armWithAlpaca` needs `alpaca.enabled`). The firmware and demo only change arming on Alpaca connects when the rule is active (`dep: D-12` marker). Spec 021 renames/rewords the arming model; it adopts the rule by changing the setting path in the catalogue.

## R11 - Restart-pending settings (D-35)

- **Finding**: on the device the rain sensor and the anemometer are reconfigured as soon as settings are saved (`saveConfigCallback` in `src/main.cpp`); only GPS and Bluetooth start at boot (`web/src/components/settings/restart.ts` agrees). The spec's "rain, wind pins" in D-35 is narrower than written: the rain/wind *switches* apply live.
- **Decision**: Facts include what the device is running since boot (`running.gps`, `running.bluetooth`). GPS or Bluetooth switched on but not running is reported "starts after a restart", and every dependent inherits that link in its chain. The fix action restarts the device.

## R12 - Runtime dependencies

- WiFi connected (D-03 internet channels, D-28 NTP, D-36 mDNS), broker connected (D-02), clock set (D-24), GPS fix (D-26) and a pending restart (D-35) are facts that never block a toggle; they show as inactive with a runtime reason.
- Missing hardware (D-06/07/17/18/19/29) *does* block switching on, as it did before (constitution V: "settings that depend on missing or disabled hardware MUST be disabled with the reason and a link").
- Values typed in once a setting is on - the OTA password (D-32), the pairing passkey and a paired phone (D-08/D-31) - don't block either.

## R13 - Unknown

- **Decision**: Before the report (or status) has loaded, runtime/hardware links are *unknown* and never make a setting inactive; config-only links (e.g. MQTT off) are evaluated from the draft straight away, which keeps today's blocked-toggle behaviour.

## R14 - Re-run of the audit (spec: "MUST re-run the audit")

- Confirmed every "Today" value in the spec's table against the code at `f9990e6`.
- **Added D-36**: mDNS advertising needs a WiFi connection (runtime; never in setup-hotspot mode). Reported, not blocked.
- **Considered and excluded**: per-sensor parameters (pins, baud, poll interval, debug UART) - they have no on/off state of their own and are hidden under their sensor's switch; webhook "skip certificate checks" - only shown for `https://` URLs and harmless otherwise.
- **Constraints confirmed**: D-27 ("At least one time source must be enabled"), D-33 ("MQTT broker and topic are required when MQTT is enabled"), D-34 ("HTTP auth password is required when auth is enabled") are rejected by `Config::validate`.
  - The web Zod schema used different wording for all three, and also required an auth *username* the device doesn't. FR-010 needs the same message, so the UI now uses the device's texts and drops the username rule.
  - "Time sources must be different" can't be reached through the API: `Config::applyJson` normalises the secondary source before validating. The UI keeps the check (with the device's wording) so the form can't show an impossible combination.
  - D-32 (OTA without a password) was a UI-only rejection; per R5 it's now a dependency on both sides.
