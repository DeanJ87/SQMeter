# Audit: current UI against the DS rules

Main at `940c302` (after #115), demo build, 1280 px and 390 px. Screenshots are in [audit/](audit/);
proposed designs in [mockups/](mockups/) (branch `mock/026`, not for merge).

| # | Where | Violation | Rule | Screenshot | Proposed fix | Outcome |
|---|---|---|---|---|---|---|
| A1 | Dashboard, top | A full-width "glance strip" sits between the header and the cards: a run-on line joined with "·" ("Live · Safe · Sending alerts · Imaging app (safety monitor): Connected, last checked 1 s ago · …"), plus full-width problem bars | DS-01, DS-24 | `01-dashboard-healthy-desktop.png`, `02-glance-healthy-*.png`, `03-glance-imaging-connected-*.png` | Status card (FR-001..003), mockups `a1`..`a4` | Fixed: Status card (`web/src/dashboard/StatusCard.tsx`) |
| A2 | Dashboard, top | Problem bars repeat what cards show (e.g. "Unsafe – Sensor fault" above the Safety card saying the same) | DS-08 | `06-dashboard-sensor-fault-desktop.png` | One row per problem inside the Status card; Safety card keeps its own reasons | Fixed: one row per problem; Safety card keeps its reasons |
| A3 | Dashboard, top | "Demo" marker chip in the glance line | DS-01 | `01-dashboard-healthy-desktop.png` | Drop; the Demo panel button already says it's the demo | Fixed: "Clock moved" on the Demo button only when the demo clock is moved |
| A4 | Device & Network card | IPv6 addresses run together under one label; long addresses wrap mid-value; layout differs from the System page's WiFi card, which already lists `IPv6 (link-local)` / `IPv6 (global)` as rows | DS-05, DS-27 | `04-device-network-*.png`, `12-system.png` | One row per address with scope label and "?" (FR-005), mockup `c1` | Fixed: one row per address, scope label, "?" for local/link |
| A5 | Device & Network card | IPv4 shown in a tile while IPv6 and the local name are rows below - two patterns for one kind of value | DS-03, DS-05 | `04-device-network-desktop.png` | All addresses as rows; the tile keeps Uptime only | Fixed: IPv4 is a row; tiles are Wi-Fi and Uptime |
| A6 | Sun & Moon card | Paragraph under the chart: "Times in this browser's time zone (Europe/London), not the location's" | DS-04, DS-26 | `05-sun-moon-*.png` | Card "?" hint, mockup `d1` | Fixed: card "?" hint |
| A7 | Header (every page) | Language download banner in the header, away from the control that started it; long two-sentence help text; three buttons | DS-06, DS-22 | `08-language-downloading-*.png`, `09-language-failed-*.png` | In-card progress row + ProgressMeter + one-line Note with Retry and "?" (FR-004), mockups `b1`, `b2` | Fixed: in-card row with Pill + ProgressMeter; Note with Retry and "?" |
| A8 | Every page after a reload | Page-wide notice "The chosen language couldn't be loaded, so the UI is in English: …" | DS-06, DS-07 | `10-language-unavailable-after-reload-*.png` | Language card Note + one Status card row | Fixed: page-wide notice removed; Language card Note + Status row |
| A9 | Settings → Language | On-page sentence "Alerts the device sends (ntfy, Pushover, webhook, MQTT, Bluetooth) stay in English - write your own alert wording under Alerts to change them." | DS-04, DS-22 | `09-language-failed-phone.png` | Into the field's "?" hint, shortened | Fixed: merged into the field "?" hint |
| A10 | Settings → Alerts | Status line under the schedule: "Sun at 6.3° now (device) - dark in 2h 1m, 07:33 PM to 06:03 AM (this browser's time, Europe/London)." - several facts in one line, parentheses restating context | DS-24, DS-26 | `19-settings-alerts.png` | Two rows (Sun now / Dark from-to); time-zone detail in "?" | Fixed: Sun now / Darkness rows, zone in "?" |
| A11 | Settings → Alerts | "Phones won't ring - Needs the Bluetooth firmware build" + link, as a free line inside the events list | DS-04, DS-22 | `19-settings-alerts.png` | Note on the "Wake me" level option or a "?" on the Level header | Fixed: dependency Note "Phones won't ring: …" with the fix link (DS-04) |
| A12 | Light Sensor card / Sky Quality tile | Illuminance shown with 5 decimals ("12512.72852 lux") and wrapping inside the tile ("12512.7 / 2852") | DS-03, DS-21 | `01-dashboard-healthy-desktop.png` | Significant-figure formatting (e.g. 12 513 lux, 0.00029 lux) per spec 023 formatters | Fixed: `formatIlluminance` (whole lux from 100, 3 significant figures below) |
| A13 | Alpaca page | "Imaging app" rows read "Safety monitor: Waiting for an imaging app" - value repeats the section label | DS-21, DS-25 | `11-alpaca.png` | Value "Waiting" / "Connected" pill | Fixed: checked time + state Pill |
| A14 | System vs dashboard | Sensor names differ: System "TSL2591 Light Sensor", glance "Light sensor (TSL2591)", alerts/HA use other forms | DS-27 | `12-system.png`, `06-dashboard-sensor-fault-desktop.png` | One name per sensor from the glossary (FR-013) | Fixed: glossary names (`tools/i18n/glossary/en.json`, `lib/sensorNames.ts`); part numbers as secondary text |
| A15 | Updates page | Downgrade warning is a 188-character paragraph | DS-22 | `13-updates.png` | One line + "?" with the details | Fixed: one line + "?" |
| A16 | Settings → Time & Location | "Sun at 6.3° - not dark yet." fine; Location "?" fine - **no violation** (reference for DS-04) | - | `16-settings-time.png` | - | - |
| A17 | Dashboard links (found on the device) | In-app links were written `#/settings?…`: right for the demo's hash routing, but on the device they only changed the hash and went nowhere | DS-07 | `build/device-status-card.png` | One helper for in-app links | Fixed: `web/src/lib/appHref.ts` (path on the device, hash in the demo); checked on the spare |
| A18 | Every place a sensor is named (review) | One sensor, four names: "IR sky sensor" (Status row), "IR Temperature" (card), "MLX90614 IR" / "Sensor fault: MLX90614 IR" (safety reason, alerts); the lens alert and System card said "RG-15" | DS-27 | `build/device-dashboard.png` (before) | Glossary name everywhere, including what the firmware sends | Fixed: firmware safety reasons, alert titles ("Sensor fault: {name}") and sensor names use the glossary; IR card is "IR Sky Sensor"; System card "Rain Sensor Diagnostics". The label check now covers card titles and the device's safety reasons and alert titles (`parts` in the glossary) |
| A19 | Status card tiles (review) | Safety tile repeated the reasons under the pill; Alerts tile said "Off / Nothing is sent / Open settings" | DS-08 | `build/status-sensor-fault-*.png` (before) | Pill only; the why in "?" | Fixed: tiles are label, "?" and pill; only an action (Resume) gets a button; `sub` only names which imaging-app device |
| A20 | Sensor fault cards (review) | "Cloud cover and the cloud safety rule can't be measured." in both Cloud Conditions and IR cards | DS-08, DS-22 | `build/dashboard-sensor-fault-*.png` (before) | Said once | Fixed: a fault card is its title and pill; the effect and age are the Status row's "?" |
| A21 | Safety Monitor card (review) | "Unsafe for 11s · not shared with N.I.N.A. (Alpaca off)"; rows "Unsafe while raining - rain sensor is off" | DS-24 | `build/safety-rows-*.png` | One fact per row | Fixed: rule name as the row label, "Not in effect" pill, reason in "?"; an "Imaging apps / Not shared" row; the firmware lists rule names only. A Playwright check (`dashboard.spec.ts`, "no run-on text") fails on rendered " · " or " - " joins and on explanation under a Status tile |
| A22 | Wind card (found in the re-check) | "m/s (12 km/h)" wrapped inside the tile | DS-03 | `build/dashboard-sensor-fault-phone.png` | Two unit lines | Fixed: `MetricTile` `alt` line |

## Accepted exceptions

- Changing the firmware's reason and alert text (A18, A21) changes what MQTT, Home Assistant and Alpaca clients receive: automations should match `reasonFlags` and alert types, not text. Alert history recorded before the update keeps the old wording.
- Card titles stay title case in the source (DS-27); they display in capitals.
- `describeSchedule` joins sentences with " - "; the copy check flags only " · " run-ons (DS-24).
- `settings.alertSchedule.onlyWhileAnImagingApp` is a select option naming the mode in full (`tools/ui/copy-exceptions.json`).

All docs drift below is fixed in this change.

## Docs drift

| Doc | Says | Reality / after this spec |
|---|---|---|
| `docs/user-guide/dashboard.md` §"At a glance" | "The strip at the top … one line, for example **Live · Safe · Sending alerts · Imaging app …**" | Replace with the Status card description and screenshot |
| `docs/user-guide/dashboard.md` line 47 | "the at-a-glance line says the same" | "the Status card's Data tile" |
| `docs/user-guide/languages.md` line 10 | "A banner at the top of the page shows what the device is doing" | Progress shows in the Language card |
| `docs/development/coding-standards.md` DASH section | Refers to the glance area | Status card + DS rules |

## English strings to rewrite (then regenerate all 13 translations)

| Key | Current | Proposed |
|---|---|---|
| `glance.title` | At a glance | Status |
| `glance.imagingApp` | Imaging app ({device}): {state} | (tile) Imaging app / {state} |
| `glance.sendModeNotInEffect` | "Only while an imaging app is connected" isn't in effect: Alpaca is off, so alerts go out any time. | Imaging-app-only mode is off - Alpaca is off *(+ "?" with the rest)* |
| `glance.staleDetail` | The device's sensors haven't reported recently, so the verdict treats the data as stale. | Sensors haven't reported recently. |
| `glance.updatesStoppedDetail` | No new reading for a while - the values below may be out of date. | No new readings - values may be old. |
| `glance.disconnectedDetail` | The values below are the last ones received, not current. | Showing the last values received. |
| `glance.imagingAppSilentDetail` | It stopped checking without disconnecting - it may have crashed or lost the network. | Stopped checking without disconnecting. |
| `glance.clockNotSetDetail` / `noLocationDetail` | Night-only rules and alerts can't apply until … | Night-only rules can't apply yet. |
| `sunMoonCard.timesInZone` | Times in this browser's time zone ({zone}), not the location's | (hint) Times in {zone}, this browser's time zone. |
| `language.progressDownloading` | Downloading {language}... The page switches to it as soon as the device has it. | Downloading |
| `language.progressFailedHelp` | This firmware version ({version}) may have no language file published. Try again later, update the firmware, or upload a language file made for {version} in the Language settings. | No language file for v{version} yet. *(+ "?": Update the firmware, or upload a language file.)* |
| `language.progressRestarting` | The device isn't answering - it may be restarting. Waiting for it at this address... | Device restarting |
| `language.problemMissing` | The device doesn't have the language file (no internet, or the download failed), so the UI is in English. Download it again, or upload it from the release page. | Language file missing - showing English. |
| `language.alertsStayEnglish` | Alerts the device sends (…) stay in English - write your own alert wording under Alerts to change them. | (hint) Alerts stay in English unless you write your own wording. |
| `language.hint` | For everyone who opens this device. English is built in; other languages are downloaded from the release that matches the firmware. | For everyone using this device. Downloaded from the matching release. |
| `layout.languageUnavailableReason` | The chosen language couldn't be loaded, so the UI is in English: {reason} | (removed - see A8) |
| `updates.downgradeWarning` | {tag} is older than the installed v{version}. Older firmware may not read settings saved by a newer one; … | Older than v{version} - settings may reset. *(+ "?")* |
| `settings.alerts.soDawnBrighteningTheSky` | So dawn brightening the sky past the SQM limit doesn't wake you. At nightfall … | Skips dawn brightening. *(+ "?")* |
| `settings.alerts.quietNoSoundUrgentBreaks` | Quiet: no sound. Urgent: breaks through quiet hours. Wake me: repeats until acknowledged (…) | keep as a "?" hint; trim the parenthesis |
| `settings.alertSchedule.takesEffectStraightAwayHome` | Takes effect straight away. Home Assistant and scripts can do the same: MQTT … | Takes effect now. *(API detail → docs)* |
| `alpaca.nINAAnd` | N.I.N.A. and other Alpaca clients find this device by UDP discovery. If discovery can't reach it, add it manually with this host and port. | Imaging apps find this device automatically; if not, add this host and port. |

18 English strings exceed 110 characters today (the longest 188); the copy check (FR-012) will list
them all with their string type.
