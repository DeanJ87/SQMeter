# Feature Specification: Translations (Committed Language Files, Downloaded to the Device)

**Feature Branch**: `spec/023-i18n`

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "what I'm after is proper translations committed to the repo that I'll have AI translate every single label, text everything into various languages. and then when a person say wants SPANISH it would pull that translation file, into the littleFS storage of the device so it can display those values. It should be doable."

## Overview

SQMeter speaks English only. This feature translates **every piece of text a person reads in the SQMeter web UI** into a set of languages and keeps those translations in the repository.

- **The repository holds the translations.** All UI text is moved into one English source file. Each supported language has one translation file, with every string translated, committed and reviewed in a PR like any other change. The maintainer generates the translations with an AI tool and commits them; there is no external translation platform and no partial language.
- **The device holds English plus at most one other language.** English is built into the device's web UI. When someone picks, say, Spanish, the device downloads the Spanish file for its firmware version and stores it in its file system, and the UI shows Spanish. Picking English again deletes the file.
- **English is always the fallback.** No file, an offline device, a damaged download or a key missing from an older file all show English. They never show a blank or a raw key.

### What gets translated

| Text | Translated? |
|---|---|
| Web UI: labels, buttons, hints, notes, help tips, empty states, error and confirmation messages, page titles | Yes |
| Text the device sends that the UI shows: settings validation errors, safety reasons, sensor states, alert history entries | Yes. The UI translates it, using a message ID the device sends with it (FR-008) |
| Alerts the device sends itself (ntfy, Pushover, webhook, MQTT alert text, Bluetooth alarm) | No, they stay English (see Assumptions). Custom alert wording (spec 006/009) already lets people write their own alerts in any language |
| Machine-readable output: JSON field names and values, MQTT topics and payloads, Home Assistant discovery, the readings schema (spec 013), ASCOM Alpaca, logs | Never. Integrations must not change when someone changes language |
| The docs site (sqmeter.dev) | No (out of scope) |

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Use SQMeter in Spanish (Priority: P1)

A Spanish-speaking user opens Settings, chooses **Español** and saves. The device downloads the Spanish file and the whole UI changes to Spanish: every page, every label and message, and every error the device returns.

**Why this priority**: This is the feature.

**Independent Test**: On a device online on WiFi, choose Spanish. Go through every page, tab, dialog and message. No English text remains, apart from product names, units and values.

**Acceptance Scenarios**:

1. **Given** an English device on WiFi, **When** the user chooses Spanish and saves, **Then** the device downloads the Spanish file for its firmware version, stores it, and the UI shows Spanish everywhere within 10 seconds, without the user reloading the page.
2. **Given** a device in Spanish, **When** another browser opens it, **Then** that browser also shows Spanish. The language belongs to the device, not to the browser.
3. **Given** a device in Spanish, **When** the user enters an invalid setting, **Then** the device's error message is shown in Spanish.
4. **Given** a device in Spanish, **When** the user chooses English, **Then** the UI returns to English and the Spanish file is deleted from the device.
5. **Given** a device in Spanish, **When** the user chooses French, **Then** the French file replaces the Spanish one. Only one non-English file is ever stored.

---

### User Story 2 - English always works (Priority: P1)

Whatever happens with downloads, versions or storage, the UI is always readable.

**Why this priority**: A broken language must never lock someone out of their device.

**Independent Test**: Choose a language on a device with no internet, with a corrupted stored file, and with a file from an older release. In each case the UI works, in English or partly in English.

**Acceptance Scenarios**:

1. **Given** a device without internet, **When** the user chooses Spanish, **Then** the UI stays in English and says why: the download failed and the user can retry or upload the file by hand.
2. **Given** a stored file that is damaged or fails its integrity check, **When** the UI loads, **Then** it shows English and says the language file needs to be downloaded again.
3. **Given** a stored file missing some keys (for example, it is older than the firmware), **When** those strings are shown, **Then** each missing string appears in English and everything else stays in Spanish.
4. **Given** any state of the language file, **When** the UI loads, **Then** no raw message key, placeholder such as `{count}`, or empty label is ever shown.

---

### User Story 3 - Every string translated, kept that way (Priority: P1)

The maintainer adds or changes UI text in English. A tool generates the missing and changed translations for every language with AI, and the maintainer reviews and commits them. CI refuses any change that leaves a language incomplete.

**Why this priority**: The user wants "every single label, text everything" translated, and only an enforced check keeps it that way.

**Independent Test**:
- Add an English string without translating it; CI fails and names each language missing the key.
- Run the translation tool; it fills in every language; CI passes.

**Acceptance Scenarios**:

1. **Given** a new UI string written directly in a component instead of the English source file, **When** CI runs, **Then** it fails and names the file and line.
2. **Given** a new key in the English file, **When** a language file lacks it, **Then** CI fails and lists the missing keys per language.
3. **Given** a language file with a key English no longer has, **When** CI runs, **Then** it fails and lists the extra key.
4. **Given** a translation that drops, renames or adds a placeholder (for example, `{minutes}`), or lacks a plural form its language needs, **When** CI runs, **Then** it fails and names the key and language.
5. **Given** the English text of an existing key is changed, **When** the translation tool runs, **Then** it re-translates that key in every language. It doesn't touch keys whose English hasn't changed, so reviewed translations aren't overwritten.
6. **Given** the maintainer runs the translation tool, **When** it finishes, **Then** every language file is complete and the changes form a normal, reviewable diff for a PR.

---

### User Story 4 - Offline devices and the demo (Priority: P2)

Someone whose observatory has no internet still wants Spanish. Visitors to the demo want to try languages too.

**Why this priority**: SQMeter is often used at dark sites without internet, and the demo is the shop window.

**Independent Test**:
- Upload the Spanish file by hand to an offline device; the UI shows Spanish.
- On demo.sqmeter.dev, choose Spanish; the UI shows Spanish, and no request leaves the demo site.

**Acceptance Scenarios**:

1. **Given** an offline device and the Spanish file downloaded from the release page, **When** the user uploads it in Settings, **Then** the device checks it and uses it, exactly as if it had downloaded it.
2. **Given** a file for a different firmware version, **When** it is uploaded, **Then** the device accepts it and uses English for any keys the file lacks (User Story 2.3), and says the file is from another version.
3. **Given** the demo, **When** a visitor chooses a language, **Then** the file comes from the demo site itself. Nothing leaves the browser for any other site (spec 016 FR-006).

### Edge Cases

- **Choosing a language while the device is busy**, for example during a firmware update: the download waits and the UI says so. A language download never runs at the same time as an OTA update.
- **File system full**: the download is refused before anything is written, the UI stays in its current language, and the message says how much space is needed.
- **Interrupted download or power loss**: the new file is used only once it has been fully written and verified, and the previous language stays in use until then (all-or-nothing).
- **Firmware update**: after an update, the device downloads the file for the new version in the same language. Until that succeeds, the old file is used with per-key English fallback.
- **Filesystem image update**: an OTA or uploaded filesystem image wipes stored files. The device keeps the chosen language in its settings, which are stored separately, and downloads the file again.
- **Factory reset**: the device returns to English and deletes the file.
- **Long translations**: German and other languages run 30-40% longer than English. Layouts must wrap rather than overflow or clip at phone width.
- **Right-to-left**: no right-to-left language is in the initial set. The UI must not break if one is added later, but full right-to-left layout is out of scope.
- **Values inside text**: numbers, sensor readings and units inside translated sentences come from placeholders, so the translation can reorder them but never alters them.

## Requirements *(mandatory)*

### Functional Requirements

**Source and translation files (repository)**

- **FR-001**: Every user-facing string in the web UI MUST come from a single English source file of keyed messages. Components MUST NOT contain user-facing text literals. Product names, units and values are exempt.
- **FR-002**: The repository MUST contain one translation file per supported language. Each MUST have exactly the English file's keys, every one translated.
- **FR-003**: Messages MUST support named placeholders and plural forms, following each language's plural rules.
- **FR-004**: CI MUST fail when any of the following occurs:
  - hard-coded UI text in a component (FR-001);
  - a language file is missing a key or has an extra key;
  - a translation's placeholders differ from English;
  - a plural form required by the language is missing;
  - a file is not valid.
  The failure message MUST name the language, key, file and line as applicable.
- **FR-005**: The repository MUST include a translation tool that the maintainer runs. It sends only new keys, and keys whose English has changed since they were last translated, to an AI translation service, then writes the results into every language file. A record of the English each translation was made from lets it detect changed keys. The tool MUST NOT change translations whose English is unchanged, and MUST check its output against FR-004 before writing.
- **FR-006**: Each release MUST publish every language file as a release asset, plus a manifest listing each file's language, firmware version, size and checksum.

**On the device**

- **FR-007**: English MUST be built into the device's web UI and work with no language file present.
- **FR-008**: Text the device generates and the UI shows (validation errors, safety reasons, sensor states, alert history entries) MUST carry a stable message ID and its parameters, so the UI can translate it. The existing English text fields MUST stay in API responses unchanged, for compatibility.
- **FR-009**: The language MUST be a device setting, shared by every browser that opens the device. Changing it requires authentication when authentication is on (constitution VI).
- **FR-010**: Choosing a non-English language MUST download that language's file for the running firmware version, from the matching GitHub release. It uses the same secure download path and certificate handling as OTA updates (spec 012). The device MUST check the size and checksum against the manifest before using the file.
- **FR-011**: The device MUST store at most one language file. Installing a language replaces the previous one only after the new one is verified. Choosing English deletes it.
- **FR-012**: The user MUST be able to upload a language file by hand. It goes through the same checks as a download, except the version may differ (FR-013).
- **FR-013**: The UI MUST fall back to English for each string missing from the stored file. If the file is absent, damaged or fails its checks, the whole UI MUST fall back to English. The UI MUST say why, and offer a retry or upload.
- **FR-014**: After a firmware or filesystem update, the device MUST restore the chosen language by downloading the matching file. Until then it uses what it has, with fallback (FR-013).
- **FR-015**: Machine-readable output MUST NOT change with the language. That covers JSON keys and enum values, MQTT topics and payloads, Home Assistant discovery, the readings schema, ASCOM Alpaca, and logs.
- **FR-016**: Alerts sent by the device (ntfy, Pushover, webhook, MQTT alert text, Bluetooth) MUST stay in English, or use the user's custom alert wording when they have set it. Settings MUST say so next to the language choice.
- **FR-017**: Numbers and dates in the UI MUST be formatted for the chosen language: decimal separator, date order, 24-hour or 12-hour clock. Units remain their own setting.

**Demo**

- **FR-018**: The demo MUST offer the same languages, serving the files from the demo site. Choosing a language follows the device flow (download, store, fallback) without contacting any other site.

### Size Budgets

| Item | Budget |
|---|---|
| One language file, as stored on the device | ≤ 64 KB (compressed if the device serves it compressed) |
| Translation runtime added to the device web UI | ≤ 4 KB gzipped |
| Firmware flash for downloading, verifying and serving the file | ≤ 12 KB |
| Free file-system space needed to install a language | file size + 4 KB, checked before download |

### Key Entities

- **English source file**: every UI message, by key. The single source of truth.
- **Language file**: one per language, committed in the repository, with exactly the English keys translated, plus the language code and the firmware version it was built for.
- **Translation record**: for each language and key, the English text it was translated from. The translation tool uses it to find changed keys.
- **Language manifest**: a release asset listing each language file's code, version, size and checksum.
- **Device language setting**: the chosen language code, stored in the device settings. The setting survives filesystem updates; the file does not.
- **Message ID**: the stable key and parameters the device sends with any text the UI shows.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With any supported language chosen, an automated walk through every page, tab and dialog of the UI finds no English user-facing text, apart from product names, units and values.
- **SC-002**: 100% of keys are translated in every committed language file, and CI enforces it on every PR.
- **SC-003**: Choosing a language on a device online on WiFi takes effect in under 10 seconds.
- **SC-004**: In every failure case tested (offline, damaged file, wrong version, full file system, power loss mid-install), the UI stays usable and never shows a raw key or placeholder.
- **SC-005**: Adding one new English string and running the translation tool leaves every language complete, and CI passes with no manual edits.
- **SC-006**: MQTT, REST, Home Assistant and Alpaca output are byte-identical, apart from timestamps and readings, whatever language is chosen.
- **SC-007**: The size budgets hold for every language file and for the firmware.

## Assumptions

- **Initial languages**: Spanish (es), French (fr), German (de), Italian (it), Dutch (nl), Portuguese (pt), Polish (pl), Japanese (ja) and Simplified Chinese (zh-Hans). More can be added with one file and a tool run. All are left-to-right.
- **AI translation, human review by PR**: the maintainer runs the translation tool with their own AI service credentials. Native-speaker review is welcome through ordinary PRs, but there is no separate contributor workflow or translation platform.
- **Device alerts stay English (FR-016)**: alerts are built and sent by the device with no browser involved. Translating them would mean parsing the language file on the device at send time, which costs heap during the TLS sends that already use the most memory. Custom alert wording (spec 006/009) already lets users write their alerts in any language, so English plus custom wording is the simpler, sufficient choice. A later spec can revisit it.
- **One device-wide language**: matches how settings work today, and keeps the device to one stored file.
- **Release assets as the source**: language files are downloaded from the same GitHub releases as firmware (spec 012), so a device gets translations that match its version.
- **Sizes**: the UI has an estimated 1,500-2,500 strings; at about 25 bytes per string in a compact format, a language file is well under 64 KB.
- **Time zones**: times keep their current display rules; showing times in the device's own time zone is separate work.
- **Coding standard**: FR-001's no-hard-coded-text rule is added to the coding standard (spec 017) as a lint rule. Existing text is moved to the English file as part of this feature, not baselined.

## Dependencies

- Spec 012 (OTA updates): release assets, the TLS download path, certificate handling.
- Spec 013 (data interfaces): which output is machine-readable and must not change.
- Spec 016 (demo): nothing-outbound rule; files served from the demo site.
- Spec 017 (coding standards): lint enforcement of FR-001.
