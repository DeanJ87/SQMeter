# Implementation Plan: Firmware platform and space

**Branch**: `feat/027-arduino3` | **Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md) |
**Device results**: [device-results.md](device-results.md)

## Summary

Move both firmware builds to Arduino-ESP32 3.x (pioarduino), trim the framework with checked
`custom_sdkconfig` options, and change the partition layout in the same release: devices move once,
by USB, keeping their settings. Every image names its layout and build, so OTA refuses files for
another layout or build. CI reports both sizes against the slot and enforces SIZE-04.

## Technical context

- Platform: pioarduino `55.03.312-1` (Arduino-ESP32 3.3.12, ESP-IDF 5.5.5, GCC 14). Needs
  PlatformIO ≥ 6.2. `-Werror` applies to `src/` only (`build_src_flags`).
- Libraries: ESP32Async/ESPAsyncWebServer 3.12.0, ESP32Async/AsyncTCP 3.5.0, NimBLE-Arduino 2.5.1
  (BLE build).
- Framework options are compiled into a rebuilt framework ("hybrid compile"). pioarduino keeps
  `sdkconfig.defaults` and `sdkconfig.<env>` in the project, and the last `y` in a choice wins.
- Budgets: SIZE-02 (LittleFS) moves to 448 KB; SIZE-04 (app slot) warns at 90%, fails at 95%.

## Constitution check

- I (spec first): spec, research and the owner's clarifications exist; this plan cites them.
- IV (budgets): SIZE-04 added (constitution 1.3.0); `tools/firmware/flash_budget.py` in CI.
- VII (device verification): spare only, recorded in [device-results.md](device-results.md).
- VIII (quality): `-Werror` on our code; the quality gate passes with no new findings.

## Design decisions

| # | Decision | Why |
|---|---|---|
| D1 | Layout `l2`: nvs 0x9000/0x5000 and otadata 0xE000 unchanged; app0 0x10000 and app1 0x1D0000, 0x1C0000 each; LittleFS 0x390000/0x70000. One `partitions.csv` for both builds. | FR-015, FR-018 (NVS stays where it is) |
| D2 | Every choice in `custom_sdkconfig` lists the chosen member `=y` and the others `=n`; `scripts/clean_sdkconfig.py` drops the generated files when the option set, component list or platform changes; `scripts/check_sdkconfig.py` fails the build if an option didn't apply. | FR-010; the framework-rebuild trap |
| D3 | Trims: Arduino and IDF log levels, assertions silent, error-name table and core dump off, TLS client only with unused ciphers, curves and key exchanges off, certificate bundle CMN, WiFi extras off, unused managed components removed. WPA3 kept. | FR-004, FR-011, research R2/R3 |
| D4 | Root certificates in one `TRUSTED_ROOTS` block; each service points at the tail it trusts. | FR-005 |
| D5 | IPv6: `Ipv6Network::listen()` opens one dual-stack listener for the web server and applies the local-network peer check before a request is read; `WiFi.enableIPv6()` before connecting; TLS, HTTPClient and NTP resolve AAAA names. | FR-009, spec 015 |
| D6 | Image marker: 16 random bytes + `layout=l2;build=standard|ble;`, kept by the linker through a reference from the status and boot log. `lib/FirmwareImage` scans uploads and downloads as they stream and refuses before the boot slot changes. | FR-020 |
| D7 | Release files carry the layout (`sqmeter-l2-…`), so a 2.x device's update check never offers them; the update check keeps only asset names and sizes. | FR-020, research R7 |
| D8 | USB packages per build (`sqmeter-l2-usb-<build>-<tag>.zip`): bootloader, partition table, `boot_app0`, firmware, LittleFS, `manifest.json`, `FLASH.txt`; NVS never written. Docs host ESP Web Tools with "erase" off by default. | FR-018, FR-019 |
| D9 | Rollback: the app is marked valid at the end of `setup()`; a build with `SQM_TEST_BOOT_CRASH` proves the bootloader returns to the old slot. | FR-021 |
| D10 | `/api/status` reports `firmware.layout`; the Updates page shows a one-line note with the USB guide on a legacy layout. | FR-017, UI design system |

## Project structure (touched)

- `platformio.ini`, `partitions.csv`, `scripts/clean_sdkconfig.py`, `scripts/check_sdkconfig.py`
- `lib/FirmwareImage/`, `lib/ReleaseUrls/`, `lib/ReleaseLogic/`, `include/FirmwareMarker.h`,
  `src/FirmwareMarker.cpp`, `src/OtaUpdater.cpp`, `src/WebServerUpdates.cpp`, `src/main.cpp`
- `src/Ipv6Network.cpp`, `src/WebServer.cpp`, `src/WiFiManager.cpp`, `src/RootCertificates.cpp`
- `tools/firmware/` (budget, size map, USB package), `tools/docs/flash_assets.py`
- `web/src/components/Updates.tsx`, i18n, `web/scripts/vendor-esp-web-tools.mjs`
- Docs: `docs/getting-started/usb-flash.md`, flashing, OTA, BLE, IPv6, REST, troubleshooting,
  coding standards (SIZE-04); constitution 1.3.0; CHANGELOG
