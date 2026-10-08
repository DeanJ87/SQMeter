# Feature Specification: Internationalisation (Languages for the UI, Device Messages and Alerts)

**Feature Branch**: `spec/023-i18n`

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "I18N to avoid holding every single translation on the device, would just pull the translations on a download and hold English always, and the new language." Scope: the web UI, the text the device generates itself (alert wording, settings validation errors, safety reasons), and the demo. Also covers locale formatting, right-to-left readiness, plurals, string extraction with a CI check that no hard-coded UI strings are added (ties to spec 017), the contributor translation workflow, the docs site's language scope, and size budgets.

## Overview

SQMeter speaks English only. Every label in the web UI, every validation error, every safety reason and every alert sent to a phone is an English string. Some of these strings are written in the UI code; others are written in the device's code.

The device is an ESP32 with a 512 KB file system for the web UI, and about 300 KB of that is already used. It cannot hold every language that contributors might add. So:

- **English is always built in.** It is the source language and the fallback for everything.
- **One more language at a time.** Choosing a language downloads that language's **language pack** and stores it on the device. Choosing a different language replaces it, and choosing English removes it.
- Nothing needs a translation to work. A missing pack, a missing string, an offline device or a damaged download all fall back to English, string by string.

### Design decisions

These are recorded here because they shape the requirements. The reasoning is in Assumptions.

1. **Where device text is translated: in two places, from one pack.**
   - Text the device sends **to the web UI** (validation errors, safety reasons, alert history, sensor states) carries a stable **message ID** and its **parameters** alongside the English text. The browser translates it from the pack. This costs the device nothing and works with any pack.
   - Text that **leaves the device without a browser** (ntfy, Pushover, webhook, the human-readable fields in MQTT alerts, Bluetooth alarm text) is translated **on the device**. The pack has a small device section with the alert titles, bodies and reason phrases, and the device reads it at the moment it sends.
   - The user's custom alert wording (spec 006/009 `{variable}` templates) always wins over both the built-in English and the pack.
2. **Machine-readable output never changes language.** These stay English and stable whatever language is chosen:
   - JSON field names, enum values, MQTT topics, Home Assistant discovery IDs and the readings schema (spec 013);
   - ASCOM Alpaca names, descriptions, error messages and the SafetyMonitor and ObservingConditions texts;
   - log lines and the serial console.

   Translating any of these would break automations, N.I.N.A. and ConformU.
3. **Pack versus firmware version.** A pack is built for a firmware version. Strings are keyed by stable message IDs, and a changed meaning gets a new ID. So a pack from another version is still used: every key it has is shown, and every key it lacks falls back to English. The device fetches the pack that matches its firmware after an update. A version mismatch is never an error.
4. **Pack source.** Packs are published as assets of the same GitHub release as the firmware, next to a manifest that lists each pack's size and checksum. They are fetched the way OTA updates are fetched, over the same certificate checks. Uploading a pack file by hand covers devices without internet access.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Use SQMeter in my own language (Priority: P1)

An observer who is more comfortable in German opens Settings → Device and chooses **Deutsch**. The device downloads the German pack. Within seconds the whole UI is in German, including the dashboard, settings, alerts, Alpaca page, system page, updates and WiFi setup. It stays German after a reload, on another phone, and after the device restarts.

**Why this priority**: This is the feature. Everything else supports it.

**Independent Test**: On a device with internet access, choose a language that has a complete pack. Every page renders in that language, and switching back to English removes the pack from the device.

**Acceptance Scenarios**:

1. **Given** a device on English with internet access, **When** the user chooses a language from the list, **Then** the device downloads that language's pack, checks it, stores it, and the UI switches to that language without a page reload.
2. **Given** a device on German, **When** any browser opens the UI, **Then** it is shown in German. The language is a device setting, not a per-browser one.
3. **Given** a device on German, **When** the user chooses French, **Then** the German pack is replaced by the French one. Only one pack is ever stored.
4. **Given** a device on any language, **When** the user chooses English, **Then** the stored pack is deleted and English is used. English needs no download.
5. **Given** the language list, **When** it is shown, **Then** each language is named in its own language ("Deutsch", "Français"), and its completeness and pack size are shown.

---

### User Story 2 - English always works (Priority: P1)

The device is offline at a dark site, or GitHub is down, or the download is cut off half way, or a filesystem update has just wiped the stored pack. The UI must still be fully usable, and it must never be half-broken or blank.

**Why this priority**: The device is a safety tool used in the field, often offline. A language feature that can make the UI unusable is worse than no feature.

**Independent Test**:

- Block internet access and choose a language: the UI stays in English and says why.
- Corrupt a stored pack: the UI stays in English and says why.
- Remove keys from a pack: those strings show in English while the rest is translated.

**Acceptance Scenarios**:

1. **Given** no internet access, **When** the user chooses a language, **Then** the UI stays in its current language and shows a short note that the pack couldn't be downloaded, with a retry button and the option to upload a pack file.
2. **Given** a download that fails its checksum or size check, or isn't a valid pack, **When** it completes, **Then** it is discarded, the previous state is kept, and the failure is shown.
3. **Given** a stored pack that lacks some strings, **When** a page is shown, **Then** each missing string appears in English and the rest stays translated. No raw message IDs ever appear.
4. **Given** the device is set to German but has no stored pack (wiped by a filesystem update, or damaged), **When** the UI loads, **Then** it shows English with a note that the German pack is missing, and fetches it again when it can.
5. **Given** an uploaded pack file, **When** the device has no internet access, **Then** the pack is checked and installed exactly as a downloaded one would be.

---

### User Story 3 - Alerts on my phone in my language (Priority: P2)

A user with the device set to Spanish gets a ntfy or Pushover alert "Ha empezado a llover" instead of "Rain started". A safety alert lists its reasons in Spanish. The MQTT alert message is in Spanish too, but its event type stays `rain_started`, so Home Assistant automations keep working.

**Why this priority**: Alerts are read half-asleep on a phone. This is where the language matters most outside the UI. But it needs the pack format (US1) first.

**Independent Test**: Set a language, then send a test alert to each channel. The title and body arrive in that language, and the machine fields are unchanged.

**Acceptance Scenarios**:

1. **Given** a device on a language with a pack, **When** an alert is sent to ntfy, Pushover, a webhook, MQTT or Bluetooth, **Then** its human-readable title and body use the pack's wording, with the same variables filled in.
2. **Given** a custom alert wording set by the user, **When** that alert is sent, **Then** the custom wording is used unchanged in every language.
3. **Given** a pack that lacks an alert's wording, **When** that alert is sent, **Then** the built-in English wording is used.
4. **Given** any language, **When** MQTT, webhook or Home Assistant payloads are published, **Then** field names, event types, topics and enum values are the same as in English.
5. **Given** an alert with a count in it ("3 reasons"), **When** it is sent in a language with several plural forms, **Then** the correct plural form for that count is used.

---

### User Story 4 - The device's own messages in my language (Priority: P2)

A user enters an invalid MQTT port. The error comes from the device's own validation, and it appears in their language with the value in it. The safety card's reasons ("Rain detected", "Cloud cover 92% is above 90%"), the alert history and the sensor states are translated too.

**Why this priority**: Without this, a translated UI keeps showing English device messages in exactly the places that matter: errors and safety.

**Independent Test**:

- Trigger each validation error and each safety reason in another language. Every one is translated with its values in place.
- Remove one from the pack. It shows the device's English text.

**Acceptance Scenarios**:

1. **Given** the device rejects a setting, **When** it replies, **Then** the reply carries a stable message ID and its parameters as well as the English message, and the UI shows the translation from the pack with the parameters filled in.
2. **Given** a safety verdict with reasons, **When** it is shown in the UI, **Then** each reason is translated from its ID and parameters.
3. **Given** a device message with no translation in the pack, **When** it is shown, **Then** the device's English text is shown.
4. **Given** an older client or a script, **When** it calls the API, **Then** the English text is still in the same fields as before. Adding message IDs is backwards compatible.

---

### User Story 5 - Numbers, dates and times look right (Priority: P3)

In German, 21.5 °C shows as "21,5 °C", the date shows as "8. Okt. 2026", and the time uses a 24-hour clock. The units (°C or °F, mm or inches) do not change with the language; they stay the separate units setting.

**Why this priority**: Expected by anyone using their own language, but the UI is still usable without it.

**Independent Test**: Switch languages. Numbers, dates, times and durations follow the language's conventions, and units follow the units setting.

**Acceptance Scenarios**:

1. **Given** a language is chosen, **When** numbers, dates, times and durations are shown, **Then** they use that language's separators, order and clock.
2. **Given** a language is chosen, **When** units are shown, **Then** they follow the units setting, not the language.
3. **Given** a number input in a language that uses a decimal comma, **When** the user types "21,5", **Then** it is understood as 21.5. Typing "21.5" works too.
4. **Given** times shown for the device's location, **When** they are labelled with a time zone, **Then** they use the device's time zone, not the browser's.

---

### User Story 6 - Contributors add and maintain languages (Priority: P3)

A contributor who speaks Polish adds Polish by opening a pull request. The pull request adds one translation file, and CI says what is missing, what is unused and what is broken (a dropped `{variable}`, for example). When a developer adds a new English string, CI fails if they wrote it into a component as a literal instead of a message ID.

**Why this priority**: Languages only stay usable if keeping them up to date is routine. It doesn't block a first language, though.

**Independent Test**:

- Add a pack with a missing key, an extra key and a broken placeholder. CI reports all three.
- Add a hard-coded English sentence in a component. CI fails with the rule ID.

**Acceptance Scenarios**:

1. **Given** a new language file in a pull request, **When** CI runs, **Then** it reports the file's completeness and fails on a malformed file, a placeholder that doesn't match English, or broken plural forms.
2. **Given** a user-visible literal string added in UI code, **When** CI runs, **Then** it fails under the coding-standards gate (spec 017). Existing literals are baselined and burned down.
3. **Given** an English string is removed or its meaning changes, **When** a developer edits it, **Then** the change uses a new message ID, and the translations of the old ID are reported as unused.
4. **Given** a release, **When** it is built, **Then** a pack is produced for every language at or above the minimum completeness, plus the manifest, and attached to the release.

---

### User Story 7 - Languages in the demo (Priority: P3)

The demo (demo.sqmeter.dev) offers the same languages as a real device. Packs are served from the demo site itself; nothing is fetched from GitHub or any other third party (spec 016, FR-006).

**Why this priority**: It lets people see SQMeter in their language before they build one, and it lets translators check their work without a device.

**Independent Test**: In the demo, choose each language. The UI switches, and the network log shows only same-origin requests.

**Acceptance Scenarios**:

1. **Given** the demo, **When** a language is chosen, **Then** the pack loads from the demo site and the emulated device uses it for its own messages and alerts.
2. **Given** the demo, **When** any language action runs, **Then** no request leaves the demo's origin.

---

### Edge Cases

- **A firmware update changes the version.**
  - The stored pack is kept and used with per-string fallback.
  - The device fetches the matching pack when online.
  - If the new release has no pack for that language (it fell below the minimum completeness), the old pack is kept and the UI says it may be incomplete.
- **A filesystem (web UI) update replaces the whole file system.** The pack is restored afterwards: either kept across the update or downloaded again. Until then, English with a note, as in US2-4.
- **A download is interrupted, or the device restarts mid-install.** The previous pack (or English) stays active. A half-written pack is never used. Installation is all-or-nothing.
- **Not enough free space on the file system.** The install is refused before anything is deleted, with the space needed and the space free.
- **The pack is changing while alerts are being sent.** An alert being sent while the pack is replaced uses either the old or the new wording, never a mix or garbage. If neither can be read, it uses English.
- **Hostile content.**
  - A malicious or oversized pack is rejected by the size cap and checksum.
  - Pack text is only ever shown as text, never as markup.
  - Placeholders can't inject markup or break out of a field.
- **Long translations.** German and Finnish strings can be 30–40% longer. Layouts must wrap rather than overflow at phone width, and buttons must not clip.
- **Right-to-left languages.**
  - With an RTL pack, the page direction flips and layouts stay usable.
  - Numbers, sensor values, units, IPs and code stay left-to-right inside RTL text.
- **The language changes while a page is open.** Other open browsers pick up the change on their next status update; they don't need a reload.
- **A pack with the wrong language code, or from another project.** It is rejected.
- **The captive-portal WiFi setup page.** The device has no internet access then. It uses the stored pack if there is one, otherwise English.
- **Alpaca setup pages and Alpaca error strings.** These stay English (decision 2), even though the setup redirect lands in the translated UI.

## Requirements *(mandatory)*

### Functional Requirements

**Languages and packs**

- **FR-001**: English MUST be built into the firmware and the web UI and MUST always be complete. It MUST never need a download.
- **FR-002**: The device MUST store at most one language pack besides English. Installing a pack MUST replace the stored one. Choosing English MUST delete it.
- **FR-003**: The chosen language MUST be a device setting. It is part of the device's configuration, saved with it, included in backups, and applies to every browser and to off-device alerts.
- **FR-004**: The language list MUST come from the release's pack manifest. Each entry shows the language's own name, its completeness percentage and its size. When the manifest can't be fetched, the list MUST still offer English and the installed language.
- **FR-005**: A pack MUST record:
  - its language code;
  - its direction (left-to-right or right-to-left);
  - the firmware version it was built for;
  - its completeness;
  - its plural rules;
  - a UI section and a device section (alert wording, reason phrases, sensor and state names used in off-device text).
- **FR-006**: The device MUST download packs only from the release source it already uses for firmware updates, over the same certificate checks (spec 012). It MUST verify each pack's size and checksum against the manifest before installing it.
- **FR-007**: Users MUST be able to install a pack by uploading the file, for devices without internet access. The same validation applies as for downloads, except that the manifest checksum is replaced by structural validation, the size cap and the language-code check.
- **FR-008**: Installing a pack MUST be all-or-nothing. A failed, interrupted or invalid install MUST leave the previous pack (or English) active and report why.
- **FR-009**: Changing the language, installing a pack and deleting a pack MUST require authentication (constitution VI, `requireAuth`).
- **FR-010**: After a firmware update, the device MUST fetch the pack that matches its new version when it has internet access. Until then it keeps the previous pack with per-string fallback. After a filesystem update, the device MUST restore the pack, either by keeping it or by downloading it again.

**Fallback**

- **FR-011**: Every string MUST fall back to English individually when it is missing from the pack, empty, or has placeholders that don't match English. Raw message IDs MUST never be shown to users.
- **FR-012**: When the chosen language's pack is missing or unreadable, the UI MUST work fully in English and show one short note saying why, with retry and upload actions (constitution V: short notes, no paragraphs).
- **FR-013**: The language feature MUST NOT delay the first render of the UI in English. A pack that loads slowly or fails MUST NOT block the UI.

**Device-generated text**

- **FR-014**: Every user-facing message the device sends to the UI MUST carry a stable message ID and its parameters as well as the existing English text. This covers validation errors, safety reasons, alert titles and bodies in the alert history, sensor and connection states, and update and WiFi status messages. Existing English fields MUST remain, so the API stays backwards compatible (spec 013).
- **FR-015**: The UI MUST translate device messages from their ID and parameters, and fall back to the English text the device sent.
- **FR-016**: Alerts sent off the device (ntfy, Pushover, webhook, the human-readable MQTT alert fields, Bluetooth alarm text) MUST use the pack's device section for their title, body and reason phrases. They fall back to English per string. The user's custom wording overrides both.
- **FR-017**: Device-side text with counts MUST use the language's correct plural form.
- **FR-018**: Machine-readable output MUST NOT change with the language. This covers:
  - JSON field names and enum values;
  - MQTT topics and event types;
  - Home Assistant discovery;
  - the readings schema;
  - ASCOM Alpaca names, descriptions and errors;
  - logs and the serial console.
- **FR-019**: Reading the device section MUST NOT block the main loop or the web server's request handling. It MUST stay within the heap and stack budgets (constitution IV), for example by reading only the string needed, when needed.

**Formatting and layout**

- **FR-020**: Numbers, dates, times, durations and relative times in the UI MUST be formatted for the chosen language. Units MUST follow the separate units setting.
- **FR-021**: Number inputs MUST accept the chosen language's decimal separator as well as a dot.
- **FR-022**: Times labelled with a time zone MUST use the device's time zone setting, not the browser's.
- **FR-023**:
  - The page MUST declare its language and direction.
  - With a right-to-left pack, the layout MUST remain usable at phone and desktop widths. No overlapping, clipped or unreachable controls.
  - Values that must stay left-to-right (numbers with units, IPs, hostnames, topics, code) MUST be kept left-to-right.
- **FR-024**: Layouts MUST cope with translations up to 40% longer than English without overflow at phone width (constitution V).
- **FR-025**: Pack text MUST be rendered as plain text only. Formatting such as emphasis or links MUST come from the UI's own markup around translated segments, never from the pack.

**Tooling and workflow**

- **FR-026**: User-visible UI strings MUST be referenced by message ID from an English source catalogue. CI MUST fail when a new user-visible literal string is added to UI components. Existing literals MUST be tracked in the spec 017 baseline and burned down.
- **FR-027**: CI MUST check every translation against English:
  - the file is well-formed;
  - no keys are unknown;
  - placeholders are identical;
  - plural forms are valid for the language.

  It MUST report missing keys and completeness without failing. It MUST fail on malformed files and placeholder mismatches.
- **FR-028**: CI MUST fail when the device and the UI disagree on message IDs. Every ID the device can emit MUST exist in the English catalogue, and every catalogued device ID MUST be emitted somewhere or be marked reserved.
- **FR-029**: The release workflow MUST build a pack for every language at or above the minimum completeness (default 80%), plus the manifest with sizes and checksums, and attach them to the GitHub release next to the firmware.
- **FR-030**:
  - Translations MUST live in the repository as one file per language.
  - Contributors add or update them by pull request.
  - CONTRIBUTING and the docs MUST describe how to do this.
- **FR-031**: A changed meaning MUST get a new message ID. Changing only the English wording keeps the ID, and marks the translations for review.

**Demo**

- **FR-032**: The demo MUST offer the same languages. It serves packs from its own origin and makes no third-party requests (spec 016, FR-006). The emulated device MUST use the pack's device section for its alerts and messages as a real device would.

**Docs**

- **FR-033**: The docs MUST describe choosing a language, offline use and pack upload, what stays English and why, and contributing a translation (constitution VII).

### Size Budgets

| Item | Budget |
|---|---|
| Runtime code added to the web UI | ≤ 4 KB gzipped |
| English catalogue added to the web bundle | ≤ the English text it replaces, plus 10% |
| One stored pack on the device file system | ≤ 64 KB, stored compressed where possible |
| Firmware flash added | ≤ 20 KB (constitution IV) |
| Firmware heap while sending a translated alert | ≤ 2 KB above the English path |

The CI pack builder MUST fail a language whose pack exceeds its budget.

### Key Entities

- **Language pack**: one language's translations for one firmware version. It holds the language code, the language's own name, the direction, the firmware version, completeness, plural rules, a UI section and a device section. It is stored on the device (at most one) and published per release.
- **Pack manifest**: published with each release. It lists each available pack's language code, own name, completeness, size and checksum.
- **English catalogue**: the source of truth for every message ID, its English text and its placeholders. Built into the UI and the firmware.
- **Message ID**: a stable key for one meaning of one user-facing message. Device replies carry it with its parameters.
- **Language setting**: part of the device configuration. Either English or a language code.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With a complete pack installed, 100% of user-visible text on every page of the UI is in that language. The only exceptions are the documented machine-readable values. This is checked by an automated pass over every page in a pseudo-language that marks every translated string.
- **SC-002**: Switching language takes under 10 s on a device with internet access, and no page reload is needed.
- **SC-003**: In every failure test (offline, bad checksum, truncated download, restart mid-install, wiped file system, missing keys), the UI remains fully usable, and no raw message ID or blank label is shown.
- **SC-004**: A test alert to each channel in a non-English language arrives with a translated title and body. Its MQTT event type and field names are byte-identical to the English ones.
- **SC-005**: All size budgets hold, and CI fails a change that breaks one.
- **SC-006**: A contributor can add a new language with a single pull request containing one file and no code changes, and CI reports its completeness.
- **SC-007**: Within one release of adoption, the baseline of hard-coded UI strings only goes down, and no new literals are added.
- **SC-008**: With a right-to-left pseudo-pack, every page passes the phone-width layout checks with no overlapping or clipped controls.
- **SC-009**: The demo's language switching makes zero requests outside its own origin.

## Assumptions

- **Why translation happens in two places.** If the device holds the pack and translates everything, it pays for strings it doesn't need. If the UI translates everything, alerts sent while no browser is open (the most common case for alerts) can't be translated. So the UI translates what passes through it (cheap, flexible), and the device translates only what it sends on its own: a small section of alert wording that already uses `{variable}` templates (specs 006 and 009).
- **One device-wide language, not a per-browser one.** Off-device alerts need one language, and there is only one pack slot. A per-browser override could come later, for the installed language and English only.
- **Packs come from GitHub releases.** This is the trust path the firmware already uses (spec 012), so no new host, certificate or service is added. Self-hosting is covered by uploading the file.
- **Contributor workflow is pull requests with JSON files in v1.** No third-party service is required. A hosted translation platform (such as Weblate's free tier for open-source projects) can be connected to the same files later without changing the format.
- **The docs site (sqmeter.dev) stays English** in this spec. Translating the docs is a separate, later decision.
- **No initial set of languages is promised.** The first release ships the machinery and a pseudo-language for testing. Real languages arrive as contributors provide them.
- **Minimum completeness to publish a pack: 80%.** The rest falls back to English. The threshold can be changed in the release configuration.
- **Language-independent names.** Sensor model names (TSL2591, MLX90614, BME280, RG-15), "SQM", "Bortle", "NELM" and "N.I.N.A." are names, not translated text.
- **Units remain a separate setting** (`units`: metric, imperial or switch). Language never changes units.
- **Depends on:**
  - spec 012 (OTA release source and certificates);
  - spec 013 (API contracts, which stay backwards compatible);
  - spec 016 (demo, nothing outbound);
  - spec 017 (lint gate and baseline for the no-literal-strings rule);
  - constitution IV (budgets), V (UI), VI (auth) and VII (docs).
