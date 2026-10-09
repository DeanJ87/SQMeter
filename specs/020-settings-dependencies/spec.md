# Feature Specification: Settings Dependencies

**Feature Branch**: `spec/020-settings-dependencies`

**Created**: 2026-10-08

**Status**: Implemented (PR #97) - converged

## Background: what the audit found

Today each screen handles dependencies its own way:

- **Some toggles are blocked.** A toggle can't be switched *on* while its dependency is off, but it stays on if the dependency is switched off later. Examples: MQTT alert channel, the publish groups, night-only sky alerts, and the rain, wind and sensor safety rules.
- **Some only warn.** Rain and sky alert events can be set to a level, with a note underneath.
- **Some are only validated on the device.** Time sources and MQTT broker/topic are rejected when invalid.
- **Some are not handled at all.**

The device itself accepts every combination. It then either quietly skips the setting at run time or doesn't check it at all. Nothing reports whether a switched-on setting is actually in effect.

The defaults themselves produce "on but inactive" settings. Publish rain/wind/GPS defaults to on while those sensors default to off.

## Clarifications

### Session 2026-10-08

No interactive session was held. The decisions below are documented under Assumptions instead:

- the chosen behaviour (saved-but-inactive, not rejected or auto-disabled)
- the difference between dependencies and invalid combinations
- the scope of the catalogue

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Nothing claims to be on when it can't work (Priority: P1)

An observer switches MQTT off while the MQTT alert channel is on. The channel is shown as **inactive**, with the reason ("MQTT is off") and a one-click way to turn MQTT back on. The alerts card no longer counts it as a working channel. If MQTT was the only channel, they're warned that alerts reach nowhere. The device's status reports the channel as inactive, so Home Assistant, scripts and the demo agree with the UI.

**Why this priority**: A setting that looks on but does nothing is the worst failure for a safety device. The observer believes they'll be alerted to rain and they won't be.

**Independent Test**: For any catalogued dependency, switch the dependency off and check three things:
- the dependent setting shows "inactive because …" with a fix link
- the device's effective-state report marks it inactive
- the device does not act on it (no publish, no alert attempt, no rule evaluation)

**Acceptance Scenarios**:

1. **Given** MQTT is on and the MQTT alert channel is on, **When** the user switches MQTT off and saves, **Then**:
   - the MQTT alert channel shows "Inactive - MQTT is off" with a "Turn on MQTT" action
   - the channel count excludes it
   - the device reports `mqtt` as an inactive alert channel with reason `mqtt-off`
   - no MQTT alert delivery is attempted or recorded as "failed"
2. **Given** the rain sensor is off, **When** the user opens Settings → Alerts, **Then** "Rain starts" and "Rain stops" can't be raised from Off. If they were already set, they show as inactive with the reason, and the device never raises them.
3. **Given** "Unsafe while raining" is on and the rain sensor is off, **When** the safety verdict is evaluated, **Then** the behaviour is the one documented for that rule and shown on the rule (see catalogue D-15). Silently ignoring it is not allowed.
4. **Given** "On while N.I.N.A. is connected" is on and Alpaca is switched off, **When** the user views the setting, **Then** it shows "Inactive - Alpaca is off" with a fix link, and the device does not change alert arming on Alpaca connects that can't happen.

---

### User Story 2 - Switching something off never loses my setup (Priority: P1)

An observer temporarily switches off the rain sensor for maintenance. Their rain alert levels, rain safety rules and rain MQTT publishing are all kept. When the sensor is switched back on, everything works exactly as before, with nothing to re-enter.

**Why this priority**: Auto-clearing dependent settings punishes routine maintenance. People would be re-doing their alerts every time they unplug a sensor. Rejecting the save would make it impossible to switch the sensor off at all.

**Independent Test**: For every catalogued dependency, record the dependent settings, then:
1. switch the dependency off, save and restart
2. switch it back on and save
3. check that the dependent settings are identical and active again.

**Acceptance Scenarios**:

1. **Given** rain alerts at "Wake me", "Unsafe while raining" on and MQTT publish rain on, **When** the rain sensor is switched off, saved, then switched on and saved, **Then** all three settings are unchanged and active.
2. **Given** a dependency is off, **When** the device's settings are exported or read back through the API, **Then** the dependent values are present, unchanged, and marked inactive in the effective-state report.

---

### User Story 3 - The same answer everywhere (Priority: P2)

Whether a setting is effective is decided once, by the device's own logic. The web UI, the device's API, MQTT/Home Assistant and the demo all show the same answer. The UI never decides "active" by itself from guesses about the device.

**Why this priority**: Today the UI derives availability from a mix of unsaved settings and live status, and the device has no notion of it. This has already produced disagreements, for example a channel counted as working when its transport is off.

**Independent Test**: For each catalogued dependency, set up the inactive combination through the API only (no UI). Then check that the UI, the effective-state report and (in the demo) the emulated device all show it as inactive with the same reason.

**Acceptance Scenarios**:

1. **Given** a settings change saved through the API that leaves a dependent setting inactive, **When** the Settings page loads, **Then** the inactive note matches the device's reported reason.
2. **Given** the demo, **When** any catalogued dependency is switched off, **Then** the demo reports exactly what a real device reports, because it runs the same logic.

---

### User Story 4 - New settings can't forget their dependencies (Priority: P2)

A contributor adds a new setting that only works when something else is on. The project's checks fail until the dependency is declared in the catalogue and has tests. Convergence (`/speckit-converge`) can verify the catalogue against the code.

**Why this priority**: The audit found many gaps because nothing forced anyone to think about dependencies. Without a check, new gaps will appear.

**Independent Test**: Add a dummy setting that reads another setting's switch at run time without declaring it. The dependency check fails and names the setting. Declare it and add the tests, and the check passes.

**Acceptance Scenarios**:

1. **Given** a new setting with an undeclared dependency, **When** the checks run, **Then** they fail and name the setting.
2. **Given** a catalogue entry without device-logic and UI tests, **When** the checks run, **Then** they fail and name the entry.

---

### User Story 5 - Invalid combinations are still refused, clearly (Priority: P3)

Some combinations aren't a dependency that can wait: they can't work at all. Examples are no time source, or both time sources the same. These stay rejected with the device's own message, as today. The UI explains them before saving.

**Why this priority**: These are already handled, but they need to be distinguished from dependencies so the two aren't confused.

**Acceptance Scenarios**:

1. **Given** NTP and GPS are both off, **When** the user saves, **Then** the save is refused with "At least one time source must be enabled", as today, and the UI shows this before saving.

### Edge Cases

- **Dependency on hardware detection, not a setting.** Examples: the BME280 is not detected, or the MLX90614 is not responding.
  - The dependent shows as inactive, with a "not detected" or "not responding" reason, distinct from "switched off".
  - It becomes active by itself when the hardware responds, with no re-save needed.
  - "Unknown" (status not loaded yet) never shows as inactive.
- **Dependency that needs a restart.** Examples: GPS, Bluetooth and the rain sensor take effect after a restart.
  - The dependent shows "inactive until restart", not "active".
  - Effective state reflects what the device is actually running, not what is saved.
- **Unsaved changes.** In the Settings page the user may switch a dependency off without saving.
  - The UI previews the consequence ("Saving makes these inactive: …") from the draft, using the same catalogue rules.
  - The authoritative state remains the device's report after saving.
- **A chain of dependencies.** Example: the HA alerts switch needs alerts on and MQTT on.
  - The reason shown is the first unmet link in the chain, in catalogue order.
  - The fix action goes to that link.
- **A dependency met in two ways.** Example: darkness needs a location in Settings or a GPS fix.
  - The setting is inactive only when no way is met.
  - The reason names the simplest fix ("Needs your location").
- **Defaults.** Shipped defaults must not produce settings that are on but inactive and shown as a warning on a fresh device.
  - Either the default is off, or the inactive state of a default is shown neutrally (muted note, not a warning).
  - Today publish rain/wind/GPS default to on while those sensors default to off.
- **Effective state in the API.** Clients that only read settings, and know nothing about effective state, keep working unchanged. Effective state is added, not substituted.
- **Test sends.** "Send test" on an inactive channel is not offered; the reason is shown instead. The same applies to "Test" on an inactive event. The device answers a test on an inactive channel with "skipped" and the reason, never "failed".

## Requirements *(mandatory)*

### Functional Requirements

**The rule**

- **FR-001**: A dependency is a setting (or piece of hardware) that another setting needs in order to have any effect. The device MUST accept and keep a dependent setting whose dependency is unmet. It MUST NOT reject the save, and it MUST NOT change or clear the dependent value.
- **FR-002**: The device MUST NOT act on a dependent setting whose dependency is unmet:
  - no publish
  - no alert evaluation or delivery attempt
  - no arming change
  - no scheduled action

  The exception is a safety rule whose documented behaviour for "dependency unmet" is to report unsafe (FR-009).
- **FR-003**: The device MUST report the effective state of every catalogued dependent setting: active, or inactive with a machine-readable reason code and a human-readable reason. The report MUST be available through the device API, and the demo MUST expose it identically.
- **FR-004**: The web UI MUST show every catalogued dependent setting in one of four states:
  - **off**
  - **active**
  - **inactive** — the toggle or level stays as saved, with a note "Inactive - <reason>" and a fix action that goes to the dependency
  - **unknown** — the status is not loaded yet; nothing is blocked or warned
- **FR-005**: In the UI, a dependent setting whose dependency is unmet MUST NOT be newly switched on or raised from Off. A setting that is already on MUST always be switchable off. *(This keeps today's blocked-toggle behaviour, applied to every catalogued dependency.)*
- **FR-006**: The UI MUST work out the inactive state and its reason from the same catalogue rules the device uses. When the device's report is loaded, the device's report wins. When the user has unsaved changes, the UI MUST preview which settings saving would make inactive.
- **FR-007**: Counts and summaries (for example "N channels", "alerts reach nowhere", the safety rules count) MUST count only active settings. When every alert channel is inactive, the UI MUST warn that alerts reach nowhere, the same as when no channel is on.
- **FR-008**: Switching a dependency off and back on MUST restore every dependent setting to active, unchanged, with no re-entry. This holds across a save and a restart.
- **FR-009**: Safety rules are the one place where "dependency unmet" may have a stricter meaning than "inactive". Each safety rule in the catalogue MUST declare one of:
  - **inactive** — the rule is ignored, and the verdict lists it under "rules not in effect"
  - **fail-safe** — the verdict is unsafe with the reason "<sensor> is off/not responding"

  The UI MUST show which one applies. The current behaviour of each rule MUST be confirmed and recorded in the catalogue during planning.
- **FR-010**: Invalid combinations (Constraint rows in the catalogue) MUST continue to be rejected by the device with its own message. The UI MUST show the same message before saving. A combination MUST be classed as either a dependency or a constraint, never both.
- **FR-011**: The demo MUST use the device's own dependency logic. The demo runs the device core, so no demo-only rules are allowed.

**The catalogue**

- **FR-012**: The project MUST keep one dependency catalogue (the table below, kept as the source of truth in the repository). For each entry it MUST record:
  - the ID
  - the dependent setting
  - what it depends on
  - its class (Dependency or Constraint)
  - for safety rules, its unmet behaviour (inactive or fail-safe)
  - the reason code and text
  - the fix target
- **FR-013**: Every catalogue entry MUST have tests that check:
  - (a) the device logic: inactive when unmet, active again when met, the value kept
  - (b) the UI: the note, the fix action, and that the setting can't be switched on but can be switched off
- **FR-014**: The project's checks MUST fail when the device code makes a setting's effect depend on another setting or on hardware state that has no catalogue entry, or when a catalogue entry lacks either kind of test. Convergence MUST be able to compare the catalogue with the code and list the differences.

### Dependency catalogue

Notation:
- "on" means the setting's switch is on.
- "detected" means the hardware is found and responding in the running device.
- *Today* records the audit result on 2026-10-08:
  - **OK** — already blocked in the UI and skipped on the device
  - **Partial** — blocked when switching on, but stays "on" with no effective-state report if the dependency is switched off later
  - **Broken** — the UI shows or counts it as working while it can't work, or there is no handling at all

| ID | Dependent setting | Depends on | Class | User sees when unmet (reason · fix) | Today |
|---|---|---|---|---|---|
| D-01 | Alert channel: MQTT | MQTT on | Dependency | "Inactive - MQTT is off" · Network → MQTT | **Broken**: counted in "N channels" and the "Turn on a channel" warning is suppressed; the device accepts it via the API; the delivery attempt is recorded at send time instead of being reported as inactive |
| D-02 | Alert channel: MQTT | MQTT broker connected | Dependency (runtime) | Status badge "Broker not connected"; deliveries "skipped - not connected" | Partial (badge only) |
| D-03 | Alert channels (Pushover, ntfy, webhook) | WiFi connected to a network (not setup hotspot) | Dependency (runtime) | "Inactive - not connected to WiFi" | Broken (not reported) |
| D-04 | All alert events and channels | Alerts on ("Send alerts") | Dependency | Section greyed: "Alerts are off" | OK |
| D-05 | Alert events: Rain starts, Rain stops | Rain sensor on | Dependency | "Inactive - rain sensor is off" · Sensors → Rain | Partial |
| D-06 | Alert event: Dew risk | BME280 detected | Dependency (hardware) | "Inactive - BME280 not detected" | Broken: the reason is computed but not shown on the row |
| D-07 | Alert events: Skies clear up, Skies cloud over | MLX90614 detected | Dependency (hardware) | "Inactive - MLX90614 not detected" | Partial |
| D-08 | Alert event level "Wake me" (rings phones) | Bluetooth build, Bluetooth on, passkey set, ≥1 paired phone | Dependency (chain) | Level kept; note "Phones won't ring - <first unmet link>" · Device → Bluetooth | **Broken**: "Wake me" is selectable and shown as ringing with no Bluetooth |
| D-09 | Sky alerts only when it's dark | Location set or GPS fix | Dependency | "Inactive - needs your location" · Time & Location | Partial |
| D-10 | Safety alerts only when it's dark | Location set or GPS fix | Dependency | "Inactive - needs your location" · Time & Location | **Broken**: not blocked or explained at all |
| D-11 | "Dark means" (sun altitude) | Either night-only option on | Dependency | Disabled while both are off | OK |
| D-12 | Alerts on/off follows N.I.N.A. (armWithAlpaca) | Alpaca on | Dependency | "Inactive - Alpaca is off" · Safety → Alpaca | **Broken**: not blocked or explained |
| D-13 | Home Assistant alerts on/off switch (discovery) | MQTT on, HA discovery on, alerts on | Dependency (chain) | Not announced; note in Network → Home Assistant | Broken (not reported) |
| D-14 | MQTT publish groups: GPS / Rain / Wind | GPS on / rain sensor on / anemometer on | Dependency | "Inactive - <sensor> is off" | Partial; **defaults on while the sensors default off** |
| D-15 | Safety rules: Unsafe while raining, Unsafe if the rain sensor fails | Rain sensor on | Dependency, unmet behaviour to confirm (FR-009); the UI currently says "Reports unsafe while on" | Rule note + fix · Sensors → Rain | Partial |
| D-16 | Safety rules: Max wind speed, Max gust | Anemometer on | Dependency (FR-009) | Rule note + fix · Sensors → Wind | Partial |
| D-17 | Safety rule: Max cloud cover | MLX90614 detected | Dependency (FR-009) | Rule note | Partial |
| D-18 | Safety rule: Min sky darkness | TSL2591 detected | Dependency (FR-009) | Rule note | Partial |
| D-19 | Safety rules: Max humidity, Min dew-point margin | BME280 detected | Dependency (FR-009) | Rule note | Partial |
| D-20 | Alpaca IsSafe / ObservingConditions served | Alpaca on | Dependency | Alpaca page: "Alpaca is off" · Safety → Alpaca | OK |
| D-21 | Alpaca rain rate | Rain sensor on | Dependency | NotImplemented in Alpaca; the Alpaca page explains why | OK |
| D-22 | Alpaca wind properties | Anemometer on (direction: wind vane on) | Dependency | NotImplemented; explained | OK |
| D-23 | Wind direction (vane) | Anemometer on | Dependency | Hidden or greyed under the anemometer | OK |
| D-24 | Rain daily total reset at a time | Rain sensor on; device clock set (NTP or GPS time) | Dependency (chain) | "Inactive - the device doesn't know the time yet" | Broken (the clock link is not reported) |
| D-25 | Sun & Moon card; darkness in alerts | Location set or GPS fix | Dependency | Card hidden; Settings note "Needs your location" | OK |
| D-26 | GPS as the location source | GPS on and GPS fix | Dependency (runtime) | "Using the location in Settings - no GPS fix" | OK (locationSource reported) |
| D-27 | Primary/secondary time source | That source on | **Constraint** | Rejected: "Time sources must be different…" / "At least one time source must be enabled" | OK |
| D-28 | NTP time | WiFi connected to a network | Dependency (runtime) | NTP status "not synced - no network" | Partial |
| D-29 | Sky calibration (dark offsets, Calibrate) | TSL2591 detected | Dependency (hardware) | Calibrate disabled: "TSL2591 not detected" | OK |
| D-30 | Bluetooth settings (on, passkey, phone alarm) | Bluetooth firmware build | Dependency (build) | "Needs the Bluetooth firmware build" | OK |
| D-31 | Phone alarm | Bluetooth on and passkey set | Dependency (chain) | "Set a passkey to turn on the phone alarm" | OK |
| D-32 | Command-line (ArduinoOTA) uploads | OTA on and an OTA password set | Dependency or Constraint (decide in plan) | Today the device silently disables OTA with no password | **Broken**: shown as on, never active, no message |
| D-33 | MQTT broker and topic | MQTT on | **Constraint** | Rejected: "MQTT broker and topic are required…" | OK |
| D-34 | HTTP auth username/password | Auth on | **Constraint** | Rejected with the device's message | OK |
| D-35 | Settings that need a restart (GPS, Bluetooth, rain, wind pins) | A restart since saving | Dependency (restart) | "Takes effect after a restart"; dependents "inactive until restart" | Partial (restart prompt only) |

Summary of the audit:
- 35 entries: 31 dependencies, 3 constraints (D-27, D-33, D-34), and D-32 to be classified in the plan.
- **9 broken**: D-01, D-03, D-06, D-08, D-10, D-12, D-13, D-24 and D-32.
- 12 partial. The other 14 are OK.

The planning phase MUST re-run the audit, add any entry this table missed, and confirm each "Today" value.

### Key Entities

- **Dependency catalogue entry**:
  - ID
  - dependent setting(s)
  - required condition(s), as a chain in order
  - class (Dependency or Constraint)
  - unmet behaviour (inactive, or fail-safe for safety rules)
  - reason code and text
  - fix target (settings tab and section)
  - test references
- **Effective state**: per dependent setting, one of off / active / inactive (+ reason code, reason text, fix target) / unknown. It is reported by the device and mirrored by the demo, and shown and previewed by the UI.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For 100% of catalogue entries, the device's effective-state report, the Settings UI and the demo agree (state and reason) in an automated test of the unmet combination.
- **SC-002**: The broken entries in the catalogue drop to zero: no setting is shown or counted as working while its dependency is unmet.
- **SC-003**: For 100% of dependency entries, switching the dependency off, saving, restarting, switching it on and saving leaves every dependent value identical (automated round-trip test).
- **SC-004**: Adding a setting with an undeclared dependency makes the project checks fail, and convergence lists it.
- **SC-005**: A fresh device with shipped defaults shows no "inactive" warnings (only neutral notes, if any).
- **SC-006**: Every inactive note in the UI has a fix action that lands on the setting that fixes it, in one click.

## Assumptions

- **Chosen behaviour: saved but inactive, not rejected, not auto-disabled.**
  - Rejecting a save whenever a dependent setting is on would make it impossible to switch a sensor or MQTT off without first undoing everything that uses it.
  - Auto-disabling would silently lose configuration (US2).
  - Saved-but-inactive with a visible reason and a reported effective state satisfies both, and matches how the UI already blocks switching on.
- **Constraints stay rejections.** Combinations the device can't run with at all (no time source, missing broker or credentials) keep today's validation.
- **Hardware detection counts as a dependency.** A sensor that isn't detected makes its dependents inactive, with a "not detected" reason distinct from "switched off".
- **Runtime conditions are included where they decide whether a setting can act.** These are WiFi connected, broker connected, clock set and GPS fix. They are reported as inactive with a runtime reason, without blocking the toggle.
- **Effective state is added to the API.** It is never a change to existing fields. Existing clients are unaffected (constitution: compatible interfaces).
- **Whether each safety rule is "inactive" or "fail-safe" when its sensor is off** is a behaviour decision per rule. It is recorded in the catalogue during planning from the current behaviour; any change to it is a separate, explicit decision, since it affects IsSafe in N.I.N.A.
- **The wording of the alert arming options is out of scope here.** "Alerts on now", "On while N.I.N.A. is connected" and a possible alert when an Alpaca client disconnects belong to a separate alerting spec. D-12 only covers its dependency on Alpaca.
- **Out of scope**: reorganising the Settings tabs; new settings; changing any default value. The exception is D-14, where the plan may either change the default or show it neutrally (SC-005).
