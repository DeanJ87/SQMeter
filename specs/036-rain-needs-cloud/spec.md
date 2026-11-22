# Feature Specification: Rain needs cloud

**Feature Branch**: `036-rain-needs-cloud`

**Created**: 2026-11-22

**Status**: Draft

## Context

The rain sensor makes the SafetyMonitor unsafe whenever it reports rain. On clear, cold nights an
optical rain gauge (the Hydreon RG-15) dews or frosts over and reports light rain from a starry
sky. The telescope parks, and imaging stops until someone switches the rain rule off by hand.

This happened on three consecutive nights in November 2026, while the sky sensor read 0% cloud
with the sky 20 °C colder than the air. On those nights the owner kept the rain rule off while the
sky was clear and switched it back on when cloud came over. Every real shower in that period
arrived with cloud already showing.

Rain cannot fall from a clear sky. **Rain needs cloud** is an owner-chosen setting: rain then
only makes the observatory unsafe when the sky sensor also sees cloud, or when the sky sensor
can't be trusted. Rain reported under a healthy clear-sky reading is shown as **suspected dew**.
It does not make the observatory unsafe, and the owner can see that the rain sensor's lens needs
attention.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A dewed rain sensor no longer parks the scope (Priority: P1)

It is a clear, frosty night and the owner has turned on Rain needs cloud. At 19:43 the rain sensor
dews over and reports light rain. The sky sensor reads 0% cloud. The SafetyMonitor stays safe and
imaging carries on. The dashboard, `/api/safety` and MQTT/Home Assistant show "Rain under a clear
sky: dew suspected", with the rain intensity and the cloud reading.

**Why this priority**: this is the false park that cost hours of imaging; stopping it is the
point of the feature.

**Independent Test**: with the setting on, feed rain = true, cloud 0% and a healthy sky sensor;
the verdict is safe and the dew-suspected flag is set. With the setting off, the same inputs are
unsafe for rain (today's behaviour).

**Acceptance Scenarios**:

1. **Given** Rain needs cloud is on, the sky sensor is healthy and cloud is below the corroboration
   threshold, **When** the rain sensor reports rain, **Then** the verdict is not made unsafe by rain
   and the dew-suspected flag is raised with the measured rain intensity and cloud cover.
2. **Given** the same, **When** the rain sensor stops reporting rain, **Then** the dew-suspected
   flag clears.
3. **Given** Rain needs cloud is off, **When** the rain sensor reports rain under a clear sky,
   **Then** the verdict is unsafe for rain exactly as before this feature.

---

### User Story 2 - Real rain still closes things down (Priority: P1)

Cloud has come over (35%) and it starts to rain. With Rain needs cloud on, the SafetyMonitor goes
unsafe for rain, exactly as it would with the setting off.

**Why this priority**: the feature must never hide real rain; this is the safety half of P1.

**Independent Test**: with the setting on, rain = true and cloud at or above the corroboration
threshold gives unsafe for rain; the reason states the rain and the cloud cover.

**Acceptance Scenarios**:

1. **Given** Rain needs cloud is on and cloud is at or above the corroboration threshold, **When**
   the rain sensor reports rain, **Then** the verdict is unsafe for rain.
2. **Given** rain was flagged as dew suspected, **When** cloud rises to the threshold while it is
   still raining, **Then** the verdict becomes unsafe for rain within one evaluation, and the
   dew-suspected flag clears.

---

### User Story 3 - A broken sky sensor never hides rain (Priority: P1)

The sky sensor stops reporting (unplugged, stale data, or a fault) and it starts to rain. With Rain
needs cloud on, rain is trusted and the SafetyMonitor goes unsafe for rain.

**Why this priority**: a clear reading that can't be trusted must never overrule the rain sensor.
This keeps the fail-safe principle.

**Independent Test**: with the setting on, rain = true and the sky sensor disabled, stale or
faulted gives unsafe for rain, and no dew-suspected flag.

**Acceptance Scenarios**:

1. **Given** Rain needs cloud is on and the sky sensor is disabled, stale or faulted, **When** the
   rain sensor reports rain, **Then** the verdict is unsafe for rain.
2. **Given** Rain needs cloud is on, **When** the sky sensor becomes stale while rain is reported
   under a clear sky, **Then** the verdict becomes unsafe for rain at the next evaluation.

---

### User Story 4 - The owner can turn it on and understand it (Priority: P2)

On the Safety settings, next to the rain rule, the owner turns on **Rain needs cloud** and sets
the cloud level that confirms rain (default 10%). A "?" explains the trade-off in one or two short
sentences, in the owner's language.

**Why this priority**: the behaviour is only useful if it can be configured and understood; it
comes after the safety logic.

**Independent Test**: toggle the setting and change the threshold in the UI; the config saves,
survives a reboot, and the verdict follows it. Every new string appears translated in each
supported UI language.

**Acceptance Scenarios**:

1. **Given** a device on default settings, **When** the owner opens Safety settings, **Then** Rain
   needs cloud is off, and the threshold field is shown but inactive.
2. **Given** the rain sensor is disabled, **When** the owner opens Safety settings, **Then** the
   setting is disabled with the reason and a link to the rain sensor setting.
3. **Given** the sky (cloud) sensor is disabled, **When** the owner opens Safety settings,
   **Then** the setting is disabled with the reason, because rain would always be trusted.

---

### User Story 5 - Suspected dew is visible and can alert (Priority: P3)

When rain is flagged as dew suspected, the dashboard rain card shows it as a short note, Home
Assistant gets a dew-suspected binary sensor, and the owner can choose to be alerted, so they know
to clean or heat the lens.

**Why this priority**: useful for maintenance, but the core value is the verdict.

**Independent Test**: raise the dew-suspected state; the dashboard note, the MQTT/HA entity and
the optional alert all appear, and clear when it ends.

**Acceptance Scenarios**:

1. **Given** dew is suspected, **When** the owner views the dashboard, **Then** the rain card shows
   the dew-suspected note.
2. **Given** a dew-suspected alert is enabled, **When** dew has been suspected for the alert's
   delay, **Then** one alert is sent, and none again until it has cleared.

### Edge Cases

- **Cloud right at the threshold**: at or above the threshold counts as cloud (rain trusted); just
  below counts as clear.
- **Cloud flickering around the threshold during real rain**: once rain has been trusted, it stays
  trusted until the rain sensor's own clear-delay (hold-off) ends. A brief clear reading mid-shower
  must not flip the verdict to safe.
- **Sky sensor healthy but its cloud cover can't be computed** (for example no ambient temperature):
  this is treated as untrusted, so rain is trusted.
- **Fog or mist**: the sky sensor reads fog as cloud (warm sky), so rain in fog is trusted.
- **Rain sensor fault** (lens fault, offline): the existing rain-sensor-fault rule is unchanged and
  is not affected by this setting.
- **The rain rule itself is off** (`rainUnsafeEnabled` false): the new setting has no effect on the
  verdict, but the dew-suspected flag is still reported if the setting is on.
- **The cloud cover safety rule is off** but the sky sensor is healthy: corroboration still uses the
  sky sensor's cloud reading. The cloud rule and corroboration are independent.
- **Safety holds** (spec 035) are unaffected.
- **An older config without the new setting** loads with it off, and behaviour is unchanged.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide an owner setting, **Rain needs cloud**, default **off**.
- **FR-002**: The system MUST provide a **corroboration cloud level** in percent, default 10%, range
  1–100%.
- **FR-003**: With the setting off, rain MUST affect the verdict exactly as before this feature.
- **FR-004**: With the setting on, rain reported while the sky sensor is healthy and fresh and its
  cloud cover is below the corroboration level MUST NOT make the verdict unsafe. This rain is
  classed as **dew suspected**.
- **FR-005**: With the setting on, rain reported while cloud cover is at or above the corroboration
  level MUST make the verdict unsafe for rain, as before.
- **FR-006**: With the setting on, rain MUST make the verdict unsafe whenever the sky sensor is
  disabled, missing, stale, faulted, or its cloud cover can't be computed. A clear reading can only
  overrule rain when it is fresh and healthy.
- **FR-007**: Once rain has been trusted, it MUST stay trusted until the rain sensor reports dry
  and its existing clear-delay hold-off has ended, even if cloud drops below the level in the
  meantime.
- **FR-008**: The unsafe reason for rain MUST state the measured rain intensity, and with the
  setting on, also the cloud cover and the corroboration level (for example
  "Rain 0.4 mm/h with cloud 35% ≥ 10%").
- **FR-009**: The dew-suspected state MUST be reported as a non-unsafe flag with the measured rain
  intensity and cloud cover, in `/api/safety`, in the safety state published over MQTT (including
  a Home Assistant discovery binary sensor) and on the dashboard rain card.
- **FR-010**: The owner MUST be able to enable an optional **dew suspected** alert, using the
  existing alert delivery and delay or cooldown rules. Alerts MUST NOT change the verdict.
- **FR-011**: The rain-sensor-fault rule, safety holds, manual unsafe and every other rule MUST be
  unchanged.
- **FR-012**: The settings UI MUST put the new setting next to the rain rule on the existing safety
  card. It is disabled, with the reason and a link to fix it, when the rain sensor or the sky
  sensor is disabled.
- **FR-013**: Every new user-visible string MUST be added in English and translated into every
  supported UI language, within the design system's length limits.
- **FR-014**: Configs saved by earlier releases MUST load with the setting off. The new settings
  MUST be included in config backup and restore (spec 033) and in the REST config API.
- **FR-015**: The user guide, the REST API reference (`/api/config`, `/api/safety`) and the MQTT/HA
  reference MUST document the setting, the dew-suspected flag and the fail-safe rule.

### Key Entities

- **Rain needs cloud setting**: on/off, plus the corroboration cloud level (%). Part of the safety
  configuration next to the rain rule.
- **Dew-suspected flag**: a reported state, not an unsafe reason. It carries the rain intensity,
  the cloud cover and when it started.
- **Rain trust latch**: the rule from FR-007 that keeps trusted rain trusted until the rain
  sensor's hold-off ends.

## Design *(mandatory for UI changes)*

- **DS rules**: DS-01 (the existing safety card, no new card), DS-04 (the explanation behind "?"),
  DS-20..DS-25 (short English copy: one label, one hint, one dashboard note), and the dependency
  rule for settings whose hardware is disabled (Principle V).
- **New strings (English)**: label "Rain needs cloud"; field label "Cloud to confirm rain";
  hint/InfoTip "Rain under a clear sky is treated as dew. Always counts if the sky sensor is not
  working."; dashboard note "Rain under a clear sky: dew suspected"; alert row "Rain under a
  clear sky"; alert title "Rain sensor: dew suspected".
- **Mockup**: reviewed and approved by the owner on 2026-11-22 (Safety → Rain group on desktop and
  phone, and the dashboard rain card note). Images are kept outside the repository.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With the setting on, a replay of the 20 Nov 2026 19:43 event (rain 0.02–0.17 mm/h,
  cloud 0%, sky sensor healthy) produces no unsafe period, and a dew-suspected flag for the
  duration.
- **SC-002**: With the setting on, every replayed rain event that had cloud at or above the level,
  including the 22 Nov 2026 05:49 shower, produces an unsafe period no later than it does with the
  setting off.
- **SC-003**: With the setting on and the sky sensor disabled, stale or faulted, 100% of rain
  inputs produce unsafe, in automated tests covering each case.
- **SC-004**: An owner can find, turn on and understand the setting from the Safety settings in
  under a minute, without reading the docs.
- **SC-005**: No manual toggling of the rain rule is needed on clear frosty nights. A full clear
  night with a dewed rain sensor completes without a rain park.

## Assumptions

- The owner's sky sensor (MLX90614) is trusted for cloud. Owners who don't trust theirs leave the
  setting off, which is the default.
- "Cloud cover" means the percentage SQMeter already computes from the sky-temperature difference.
  No new sky measurement is introduced.
- The corroboration level is separate from the cloud-cover unsafe limit, because the owner may use
  a high cloud limit (for example 90%) while wanting rain confirmed at a low level (10%).
- **Constitution**: Principle I says rain is evaluated independently of other sensors' freshness,
  so a stale or faulted sky sensor can never hide rain. This feature keeps that guarantee: only a
  fresh, healthy clear reading can overrule rain, and only when the owner opts in. It does add a
  case where a healthy sky reading changes the rain outcome, so the plan records this, and the PR
  amends Principle I (MINOR) to allow an opt-in corroboration rule with that fail-safe.
- This replaces the night-watch script's practice of switching `alpaca.rainUnsafeEnabled` with the
  sky. `rainUnsafeEnabled` keeps its meaning.
- Translations are made as part of the change, following the project's existing translation
  workflow. No paid AI service is wired into CI.
