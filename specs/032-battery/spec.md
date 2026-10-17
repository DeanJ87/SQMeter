# Feature Specification: Li-ion battery and fuel gauge

**Feature Branch**: `spec/032-battery`

**Created**: 2026-10-17

**Status**: Draft

## Context

SQMeter assumes mains or 12 V power. The travel board (SQMeter-Hardware, hardware Rev A) runs from a
single-cell Li-ion pack (for example two 18650s in parallel, about 7 Ah), charged over USB-C by a
power-path charger that runs the device while it charges. It carries:

- a **fuel gauge** (MAX17048, I2C address 0x36 on the main bus) that reports state of charge (%), cell
  voltage and charge/discharge rate, with an alert output on GPIO 1;
- the **charger's status outputs** (STAT1, STAT2, PG on GPIO 2, 4 and 5, active low) that tell USB present,
  charging, charged and fault, including "too cold or too hot to charge" from the board's thermistor.

An observer in a field needs to know how much battery is left, whether it is charging, and to have the
device and their imaging software react before it runs flat. None of this exists in the firmware today.

Out of scope: other chemistries and multi-cell packs; controlling charge current or voltage from
software (set in hardware); storing battery history on the device.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See how much battery is left (Priority: P1)

The observer opens the dashboard (or glances at the OLED, spec 031) and sees the battery percentage,
whether it is charging, and roughly how long it will last.

**Why this priority**: the basic reason to monitor a battery.

**Independent Test**: on a travel board with a pack fitted, confirm the dashboard and `/api/sensors`
show state of charge, voltage, charging state and a time estimate, and that plugging in USB-C changes the
state to charging within a few seconds.

**Acceptance Scenarios**:

1. **Given** a travel board on battery, **When** the dashboard loads, **Then** a Battery card shows state
   of charge (%), cell voltage, "discharging" and an estimated time to empty.
2. **Given** USB-C plugged in, **When** the charger starts, **Then** the state changes to "charging" with
   an estimated time to full; when charging ends it shows "charged".
3. **Given** a board without a fuel gauge (every existing board), **When** the device runs, **Then** there
   is no Battery card, no battery fields beyond `battery.present: false`, and nothing else changes.

---

### User Story 2 - Get warned before it runs out (Priority: P1)

The observer wants an alert at low battery and the imaging software to park before the device dies.

**Why this priority**: a monitor that dies silently mid-session costs data and, for a safety device,
leaves imaging software trusting a value that will stop updating.

**Independent Test**: set the low threshold above the current charge and confirm a low-battery alert
fires once; enable the battery safety rule and confirm the SafetyMonitor reports unsafe with a battery
reason.

**Acceptance Scenarios**:

1. **Given** a low-battery threshold (default 20 %), **When** state of charge falls below it, **Then** a
   low-battery alert fires once through the configured alert channels and doesn't repeat until the charge
   has risen 5 % above the threshold.
2. **Given** the optional SafetyMonitor battery rule enabled with its threshold, **When** charge falls
   below it, **Then** the verdict is unsafe with the reason giving the value and the limit (constitution
   I), so connected imaging software can park and close.
3. **Given** the battery rule enabled and the fuel gauge stops responding, **When** the verdict is
   evaluated, **Then** it is unsafe with a "battery data missing" reason.

---

### User Story 3 - Stretch the battery (Priority: P2)

At critical charge the device saves power, so it keeps reporting for as long as possible and protects
the cells.

**Why this priority**: valuable on long nights, but the warning in US2 already covers the main risk.

**Independent Test**: set the critical threshold above the current charge and confirm the device applies
the configured power-saving steps and recovers when charging resumes.

**Acceptance Scenarios**:

1. **Given** power saving enabled and charge below the critical threshold (default 10 %), **When** it is
   reached, **Then** the device fires the critical alert, slows sensor reads and turns the display off,
   and reports power saving in status.
2. **Given** charge keeps falling to the shutdown level (default 5 %), **When** it is reached, **Then** the
   device sends a "going to sleep" alert through the alert channels, rings paired phones when that option
   is on (BLE build), reports unsafe for at least one safety poll, and then deep-sleeps until USB power
   returns.
3. **Given** the device asleep or power saving, **When** USB-C is connected, **Then** it wakes (or resumes
   normal operation) and a "power restored" alert is sent.

---

### User Story 4 - Integrations know the battery (Priority: P2)

Home Assistant shows the travel unit as a battery-powered device; alert templates can quote the charge.

**Why this priority**: integrations are how most users watch the device remotely.

**Independent Test**: with MQTT discovery enabled, Home Assistant creates battery-class entities for
charge, voltage and charging state; an alert template using `{battery}` renders the charge.

**Acceptance Scenarios**:

1. **Given** MQTT discovery and a fuel gauge present, **When** discovery is published, **Then** entities
   exist for state of charge (battery device class, %), voltage, charging state, and time to empty; a
   device without a battery publishes none.
2. **Given** an alert template, **When** it uses `{battery}`, `{battery_voltage}` or `{battery_state}`,
   **Then** each renders the current value.

### Edge Cases

- Fresh pack or gauge just powered: the gauge needs a short time to settle; values are reported as
  "estimating" rather than a misleading 0 % or 100 % until it has a valid reading.
- Charge fault (temperature out of range, timer, bad cell): state is "fault" with the reason, and a fault
  alert fires once.
- Pack removed while on USB: the device keeps running from USB; state shows "no battery".
- Rate near zero: time estimates show "--" rather than enormous numbers; estimates over 99 h show "> 99 h".
- Charge stuck at 99 %: the charger's "charged" status is authoritative for "full", not the percentage.
- Imaging software polling during critical power saving: Alpaca and REST keep answering; only the reading
  rate drops. Once the device deep-sleeps it stops answering: imaging software sees it disconnect, which is
  why the battery rule and going-to-sleep alert come first.
- No alert channel reachable (no WiFi in the field) at shutdown: the BLE phone alarm still rings on the BLE
  build; the device doesn't wait indefinitely for delivery before sleeping.

## Clarifications

### Session 2026-10-17 (owner)

- Q: What is the last step at critical charge? → A: Deep sleep until USB power returns, after warning.
- Q (follow-up): How is the owner warned? → A: Through the normal alert channels and, on the BLE build,
  a Bluetooth phone alarm; both are options of the battery safety rule and of the battery alert events.

## Requirements *(mandatory)*

### Functional Requirements

**Reading the battery**

- **FR-001**: The device MUST detect the fuel gauge on the main I2C bus and report `battery.present`. With
  no gauge, battery features are absent and nothing else changes.
- **FR-002**: The device MUST report state of charge (%), cell voltage (V), charge/discharge rate (%/h,
  positive when charging), and status age, refreshed at least every 10 s.
- **FR-003**: The device MUST read the charger status outputs and report the charging state: `discharging`,
  `charging`, `charged`, `fault` (with reason where known, including temperature), or `no-battery`, and
  whether USB power is present.
- **FR-004**: The device MUST report estimated time to empty while discharging and time to full while
  charging, from the rate and, when set, the pack capacity; values it can't estimate are reported as
  unavailable, not zero.
- **FR-005**: Battery monitoring MUST be on by default when a gauge is detected and can be switched off.

**Warnings and safety**

- **FR-006**: The owner MUST be able to set a low threshold (5–50 %, default 20 %) and a critical
  threshold (2–25 %, default 10 %, below the low threshold).
- **FR-007**: The alert system MUST offer battery events: low, critical, going to sleep, power restored,
  charge complete, and charge fault, each firing once per crossing with hysteresis (5 % for the
  thresholds), through the existing alert channels, levels and schedule (spec 021). Each event's level is
  settable like other events; at the **Wake me** level it rings paired phones on the BLE build. Defaults:
  critical and going to sleep at Wake me, low and charge fault at a normal level, the rest silent.
- **FR-008**: The SafetyMonitor MUST offer an optional battery rule (off by default) that is unsafe below
  its own threshold (default the critical threshold) and unsafe when the rule is enabled but battery data
  is missing or stale (constitution I), with reasons giving value and limit. The rule MUST have two
  options for when it turns unsafe: send an alert through the alert channels (default on) and ring paired
  phones over Bluetooth on the BLE build (default on).
- **FR-009**: Alert template variables MUST include `{battery}`, `{battery_voltage}`, `{battery_state}`
  and `{battery_time_left}`.

**Power saving**

- **FR-010**: The owner MUST be able to enable power saving (on by default on battery-capable boards). At
  the critical threshold it MUST fire the critical alert, slow sensor reads to a set interval and turn the
  display off (spec 031). At the shutdown level (2–10 %, default 5 %, below critical) it MUST send the
  going-to-sleep alert through the alert channels and, when enabled, the Bluetooth phone alarm; keep
  answering as unsafe for at least one safety poll interval (default 30 s) so imaging software can park;
  then deep-sleep until USB power returns. On wake (or when charge rises above critical plus hysteresis)
  it MUST resume normally and send a power-restored alert.

**What is emitted**

- **FR-011**: A `battery` object MUST be published in REST `/api/sensors` and `/ws/sensors`, MQTT
  `<base>/state`, and `/api/status` (with health and diagnostics), holding `present`, `status`, `ageMs`,
  `percent`, `voltage`, `ratePercentPerHour`, `state`, `usbPower`, `fault`, `timeToEmptyMinutes`,
  `timeToFullMinutes` and `powerSaving`.
- **FR-012**: Home Assistant discovery MUST publish battery-class entities (charge, voltage, charging
  state, time to empty) only when a gauge is present; BLE summary MUST add `battery` and `batteryState`.
- **FR-013**: The settings API, OpenAPI description, JSON schemas and the demo device MUST include the
  battery fields and settings; the demo simulates a discharging pack that charges when a demo control is
  toggled.

**UI, budgets, docs, languages**

- **FR-014**: The dashboard MUST show a Battery card and the at-a-glance view a battery tile when a gauge is
  present; the System page shows battery health; there is no card on boards without a gauge
  (constitution V).
- **FR-015**: Settings MUST gain a Battery card (monitoring switch, thresholds, pack capacity, power
  saving). Battery alert and safety rules appear in the existing alert and safety settings. Options that
  can't apply (no gauge) MUST be shown disabled with a reason and link through the settings dependencies
  catalogue (spec 020).
- **FR-016**: Configuration saved before this feature MUST load unchanged; the feature MUST stay within
  the resource budgets (constitution IV).
- **FR-017**: The documentation MUST cover the battery in the same change: a user-guide page (charging,
  reading the gauge, alerts, safety rule, power saving, matched cells and polarity), the travel board on
  the hardware pages, configuration, REST, MQTT, BLE, alerts and settings dependencies references.
- **FR-018**: Every new web UI string MUST be added to the English catalogue with its context note and
  translated into all 13 other UI languages through the existing translation workflow.

### Key Entities

- **Battery**: presence, charge %, voltage, rate, state, USB power, fault, time estimates, power-saving
  state, last update.
- **Battery settings**: monitoring on/off, low and critical thresholds, pack capacity, power-saving
  options.
- **Battery alert / safety rule**: threshold, hysteresis, enabled, last fired.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: State of charge shown by the device is within 5 % of a reference measurement across a full
  discharge, after the gauge has settled.
- **SC-002**: Plugging or unplugging USB-C is reflected in the charging state within 5 seconds.
- **SC-003**: The low-battery alert fires exactly once per crossing in a full discharge test.
- **SC-004**: With the battery safety rule enabled, imaging software sees unsafe before the cell
  protection cuts power in 100 % of discharge tests.
- **SC-005**: Boards without a gauge show no change in behaviour, and the existing contract tests and
  ConformU pass unchanged.
- **SC-006**: Every new string appears in all 14 UI languages and the docs build passes.

## Assumptions

- The gauge's state of charge is used as reported; no custom battery model is shipped.
- Pack capacity is optional; without it, time estimates come from the gauge's rate alone.
- The charger's limits (833 mA charge, 3–44 °C charging window) are set in hardware and only reported.
- Only boards that fit a gauge (the travel board) offer battery settings.

## Dependencies

- Spec 020 (settings dependencies), 021 (alerts), 023 (i18n), 028 (OpenAPI), 031 (OLED battery page).
- Board profile for the travel board (separate work) supplies GPIO 1, 2, 4, 5 for the gauge alert and
  charger status.
