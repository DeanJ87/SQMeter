# Changelog

All notable changes to SQMeter are documented here.

## [Unreleased]

### Planned

- Hardware PCB design (planned — SQMeter-Hardware repo)
- 3D-printed enclosure (planned — Printables)

## [0.3.1-beta.1] — 2026-10-10

An over-the-air update from v0.3.0-beta.1 (same layout, no USB flash). Devices still on v0.2 need the one-time USB flash described under 0.3.0-beta.1.

### Changed

- **Status card**: shows whether an imaging app (N.I.N.A. or another Alpaca client) is watching each Alpaca device, and whether alerts go out. It no longer repeats what other cards show: the verdict is on Safety Monitor, data freshness on the Sky Quality pill (now **Live**, **Stale**, **No updates** or **Offline**), and a failed sensor on its own card. Not connected is dim and only counts as something to check when alerts wait for an imaging app.
- **Safety Monitor card** shows the verdict, rain hold, Rules and History only. Rules and settings that are on but not in effect are shown in **Settings**, where they are set.
- **Alpaca management description**: `ServerName` is now the device's name (Settings → Device → Name), not "SQMeter"; `Location` is the location in use as `lat, lon` (GPS fix, else the saved location), empty when none is set; `Manufacturer` stays "SQMeter". N.I.N.A. and other clients show the device's own name.
- Settings-dependency reasons name the sensor ("IR sky sensor not detected"), not its part number, in every language.

### Fixed

- Location no longer looks stuck: the Status card showed "Location: Unknown" whenever no location was saved, and "Wake me needs the Bluetooth build" counted as a problem on every standard build.
- Settings switches tab when the address changes (an in-app link, Back/Forward or a pasted address); it used to stay on the first tab opened.
- Long tile values such as a dark sky's illuminance (0.000289 lux) step down a size instead of wrapping.
- "Silent for - safety monitor" and similar labels name the device first.
- The demo's System page shows the v0.3 partition layout.
- Docs-only pull requests can merge again: the required `build` check now reports on every pull request.

## [0.3.0-beta.1] — 2026-10-10

> **Needs a one-time USB flash.** v0.3 moves to a new partition layout. Settings, WiFi included, are kept. See https://sqmeter.dev/getting-started/usb-flash/. Devices on v0.2 don't offer this release under "Check for updates".

### One-time USB flash (v0.3)

- **Arduino-ESP32 3.x** (ESP-IDF 5.5): a maintained platform with current TLS. The secure clients
  (Pushover, ntfy, https webhooks, GitHub updates, language downloads) and time sync now reach a
  host name that only has an IPv6 address.
- **New partition layout for both builds**: two 1.75 MB firmware slots and a 448 KB web UI
  partition, using the whole 4 MB flash. **Every device needs one USB flash to move to it**; your
  settings, WiFi included, are kept. Use the browser flasher or the release's USB package:
  https://sqmeter.dev/getting-started/usb-flash/
- Release files are now named `sqmeter-l2-*` (firmware, Bluetooth firmware, web UI, USB packages).
  Devices on v0.2 don't list v0.3 under "Check for updates"; the old single-file
  `sqmeter-complete-flash-*` images are gone (they reset settings).
- The device refuses an update file made for another partition layout or the other build, before
  switching to it, and says why; a web UI file for another layout is refused before anything is
  erased.
- A new firmware is only kept once it has started properly (WiFi and the web server up); one that
  fails while starting rolls back to the previous firmware.
- `/api/status` reports `firmware.layout` (`l2`, or `legacy` when the device still needs the USB
  flash; the Updates page then says so).
- CI fails a pull request that pushes either firmware build past 95% of its app slot (SIZE-04).

### Changed

- **Sensor names in device text** use the same names as the UI: safety reasons read "Sensor fault: IR sky sensor" (was "MLX90614 IR"), "Sensor fault: light sensor", "Sensor fault: environment sensor" (was "Humidity sensor fault - humidity/dew point rules can't be evaluated"); sensor alerts are titled "Sensor fault: {name}" / "Sensor recovered: {name}"; `rulesNotInEffect` lists rule names only ("Unsafe while raining"). This is the text sent over MQTT, to Alpaca clients and in alerts: match `reasonFlags` and alert types in automations, not the text.
- **Dashboard**: a Status card replaces the at-a-glance strip, and language download progress moves into the Language card (spec 026).

### Added

- **Languages**: the web UI in Bahasa Indonesia, Spanish, French, Italian, German, Dutch, Arabic (right-to-left), Portuguese (Brazil), Polish, Japanese, Chinese (Simplified), Korean and Turkish. The device downloads the chosen language from the matching release; English is built in. Numbers, dates and decimal input follow the language.
- **IPv6** alongside IPv4: addresses on the System page and dashboard, the web UI, API and Alpaca over IPv6 from the local network, Alpaca discovery over IPv6.
- **Alert schedule**: "When to send" (any time, or only while an imaging app is connected), Pause / Resume, and new events when an imaging app stops checking, comes back or disconnects.
- **Settings dependencies**: a setting whose dependency is off is kept but shown as inactive with the reason and a fix; `/api/settings/effective` reports what's in effect.
- **Dashboard Status card**: safety, alerts, data freshness and the imaging app at a glance, with each problem listed once. Failed sensors keep their card. The rain hold counts down (`rain.clearInSeconds`).
- **Accessibility** to WCAG 2.2 AA: contrast, focus, keyboard tabs, screen-reader names and announcements.
- **Live updates**: new alerts and setting changes reach the page within seconds.
- **Demo**: set raw sensor readings, presets that follow your thresholds, a guided tour, and opt-in real test notifications.
- **Docs**: diagrams throughout, indi-allsky guide with a Node-RED flow, accessibility statement, USB flash guide with a browser flasher.

### Fixed

- Never redirect a home-network request to the setup hotspot (192.168.4.1); the hotspot only opens after 45 s without WiFi at boot.
- Stale data now detected when a sensor keeps reporting old values; the safe delay survives the 49.7-day timer wrap; the RG-15 daily reset can't be skipped by a restart.
- ObservingConditions reports "not implemented" for a sensor that isn't fitted (ConformU clean).
- The SQM calibration offset applies in twilight too.
- Custom alert wording is limited by characters, not bytes, so non-Latin text isn't cut short.
- The WiFi scan needs the password when protection is on.
- The web UI is stored gzipped on the device (425 KB to 147 KB).

## [0.2.0-beta.3] — 2026-10-08

### Fixed

- "Check for updates" failing with "Release list too large to read" from v0.2.0-beta.2 on: releases now
  ship 5 files each and the list outgrew its 6 KB buffer. It's 16 KB now, with a test that reads a full
  page of releases. Devices on v0.2.0-beta.2 need this release installed by **Manual upload** once.

## [0.2.0-beta.2] — 2026-10-08

### ⚠️ Breaking changes

- **MQTT**: readings move to one retained JSON document on `<base>/state` (default base topic `sqmeter`). Other topics: `availability`, `safe` (`1`/`0`), `safety`, `alerts`, `alerts/armed` (+ `/set`), `diagnostics`. The old per-reading payload is gone. Home Assistant users can switch on MQTT discovery instead of writing YAML. See docs/user-guide/mqtt.md.
- **`GET /api/sensors` and `/ws/sensors`** use the same document: camelCase keys, a `status` per group (`ok` / `missing` / `error` / `stale`), values only when `ok`, Unix-second `timestamp` with `timeValid`, metric units (an RG-15 in inches is converted).
- **`GET /api/status`**: per-sensor health under `sensors`, bring-up counters under `diagnostics`; `gpsData` removed. The safety object uses `safe` (was `isSafe`).
- **REST responses**: success is `2xx {"success": true, ...}`, failure `4xx/5xx {"error": "..."}` - including uploads, the RG-15 commands and the MQTT test.
- **Settings removed**: top-level `timezone`, `ntp.gmtOffsetSec`, `ntp.daylightOffsetSec` (never used; `ntp.timezone` sets the clock).
- **Default hostname** is `sqmeter` (`http://sqmeter.local`) on new installs; saved hostnames are kept.

### Added

- Home Assistant MQTT discovery (sensors, raining, observatory safety, alerts switch) and per-group MQTT publish switches
- WiFi setup screen on the captive portal (`/wifi`); mDNS (`<hostname>.local`) with an on/off setting; the hotspot keeps retrying the saved network
- Sky quality settings: averaging window, SQM offset, dark calibration (refused until the sensor is dark and the window is full)
- Dashboard guide and screenshots of every page

### Fixed

- Firmware upload failing with "Could not activate partition" on the first attempt
- Update checks ignoring beta tags; local builds report `<release>+dev`
- WiFi scan in Settings showing nothing on the first press
- Cloud cover, SQM and the safety verdict computed from one set of numbers (three different humidity fallbacks before)
- Alpaca discovery answers immediately (was up to ~1 s)
- Demo uptime, units (°C), wind highlighting against your limits, alert default wording

### Internal

- Sky, cloud, rain, safety-history, release and readings logic moved into `lib/` with native tests

## [0.0.1] — 2026-04-25

Initial alpha release.

### Firmware

- TSL2591 sky brightness sensor (SQM, NELM, Bortle 1–9)
- BME280 temperature, humidity, pressure
- MLX90614 IR cloud temperature and cloud cover estimate
- GPS module support (optional, u-blox NEO-6M compatible)
- GPS time source implementation
- NTP time sync with configurable servers and timezone
- MQTT publishing to any broker (Home Assistant, Grafana, etc.)
- OTA firmware and filesystem updates from the browser
- Captive portal Wi-Fi setup on first boot
- REST API and WebSocket live sensor feed
- Partition layout: dual OTA slots + LittleFS for web UI

### Web UI

- Real-time dashboard (SQM, Bortle, NELM, cloud cover, environment)
- System page (memory, flash, sensor status, NTP/GPS, MQTT status)
- Settings page (WiFi, NTP, GPS, MQTT, sensor intervals, I2C pins)
- OTA update page (firmware and filesystem upload with progress)
- Zod-validated settings form

### CI / CD

- GitHub Actions: firmware build on PR, release on tag
- GitHub Pages: docs + live demo deployed on merge to main
- Playwright screenshot pipeline for docs
- Complete flash image (bootloader + firmware + LittleFS) in releases
