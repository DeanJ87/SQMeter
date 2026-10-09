# Research: firmware platform and flash space

**Spec**: [spec.md](spec.md) · **Date**: 2026-10-09 · Build-only spike, nothing flashed.

Spike branches (not for merge): `spike/027-trim` (trims on 2.x), `spike/027-arduino3`
(Arduino-ESP32 3.x). Size-map tool: [tools/size_map.py](tools/size_map.py) (attributes a GNU ld
map to components).

## R1. Where the flash goes today (Arduino-ESP32 2.0.17, ESP-IDF 4.4.7)

| Build | Image | App slot | Used |
|---|---|---|---|
| Standard (`esp32dev`, `partitions.csv`) | 1,490,753 B | 1,572,864 B (0x180000) | **94.8%** (82 KB free) |
| BLE (`esp32dev-ble`, `partitions_ble.csv`) | 1,719,205 B | 1,769,472 B (0x1B0000) | **97.2%** (50 KB free) |

Standard build, by group (flash = text + rodata + IRAM + initialised data):

| Group | Bytes | Share |
|---|---|---|
| ESP-IDF (prebuilt) | 824,538 | 55.4% |
| Our code (`src/`, `lib/`) | 306,200 | 20.6% |
| Arduino and third-party libraries | 186,305 | 12.5% |
| Toolchain (libc, libm, libstdc++) | 156,386 | 10.5% |

Largest single items (standard build):

| Component | Bytes | Note |
|---|---|---|
| idf: net80211 (WiFi MAC, closed source) | 115,678 | |
| toolchain: libc | 112,114 | printf/scanf, time, string |
| idf: lwip | 105,273 | IPv4 + IPv6 |
| idf: mbedcrypto | 104,607 | |
| idf: pp (WiFi, closed source) | 57,399 | |
| lib: ESPAsyncWebServer | 56,921 | |
| idf: wpa_supplicant | 44,978 | |
| idf: mbedtls (TLS) | 44,668 | |
| idf: phy | 40,163 | |
| ours: lib/ConfigModel | 37,225 | config JSON, validation |
| lib: Arduino core | 35,924 | |
| idf: driver | 32,935 | |
| idf: esp_littlefs | 29,953 | |
| ours: src/AlertDispatcher | 29,387 | 16 KB rodata: 9 root CA certificates (PEM) |
| lib: WiFi | 29,256 | |
| ours: src/sensors | 25,979 | |
| idf: mdns | 25,745 | |
| ours: lib/DeviceCore | 23,073 | |
| ours: src/WebServerApi | 20,060 | |
| ours: lib/AlpacaLogic | 17,368 | |

BLE build adds **idf: btdm_app 125,785 B** (the Bluetooth controller, closed source) and
**NimBLE-Arduino 86,624 B**: about 228 KB on top of the standard build.

Our own code is about a fifth of the image; most of the image is the WiFi/TCP/TLS stack, which
can only be shrunk by rebuilding ESP-IDF with features switched off (possible on 3.x, R3).

## R2. Trims on 2.x (measured, `spike/027-trim`)

| Trim | Standard | BLE | Risk |
|---|---|---|---|
| `CORE_DEBUG_LEVEL=1` (Arduino core logs errors only; was 3 = info) | **−6,396 B** | **−7,596 B** | Lose Arduino core info/warn lines on serial |
| NimBLE: `CONFIG_NIMBLE_CPP_LOG_LEVEL=0`, broadcaster role off | – | **−2,396 B** | Check advertising still works (peripheral advertising is part of the peripheral role) |
| ArduinoJson `ENABLE_STD_STREAM=0`, `ENABLE_PROGMEM=0` | −68 B | – | Not worth it |
| LTO (`-flto`) | – | – | **Doesn't link**: the prebuilt 2.x framework loses `app_main` |
| Total measured | **1,484,357 B (94.4%)** | **1,709,209 B (96.6%)** | |

Estimated, not prototyped:

- De-duplicate root certificates: ISRG Root X1 is embedded twice (`AlertRootCA.h` and `GithubRootCA.h`): ~1.9 KB.
- Compile out `Logger::debug` (10 call sites): <1 KB. Info strings are useful in the field; keep.
- Disabling core dump, error-name lookup, IDF log strings, BT classic: **not possible on 2.x**: the IDF libraries are prebuilt with Arduino's sdkconfig.

**Conclusion**: on 2.x the reachable saving is about 10 KB. That buys time, not room.

## R3. Arduino-ESP32 3.x build spike (`spike/027-arduino3`)

Platform: pioarduino `55.03.312-1` (Arduino 3.3.12, ESP-IDF 5.5.5, GCC 14.2). Libraries:
ESP32Async/ESPAsyncWebServer 3.12.0, ESP32Async/AsyncTCP 3.5.0, NimBLE-Arduino 2.5.1; the rest
unchanged. Needs **PlatformIO Core ≥ 6.2** (the installed 6.1.18 refuses the platform). An
Arduino 4.0 RC (ESP-IDF 6.1) exists (`61.04.00-RC1`); not evaluated.

### Porting work found (17 compiler errors, 14 distinct changes, 11 files)

| # | Change | Where | Fix |
|---|---|---|---|
| 1 | GCC 14 deprecation warnings in Adafruit TSL2591 with our global `-Werror` | build flags | Apply `-Wall -Wextra -Werror` to our sources only (`build_src_flags`) |
| 2 | `uint32_t` is `unsigned long` on IDF 5: `%u`/`%X` formats | `Logger.cpp`, `MQTTClient.cpp` | `%lu`/`%lX` or `PRIu32` |
| 3 | `IPv6Address` removed; `IPAddress` holds v4 and v6 | `Ipv6Network.cpp` | Use `IPAddress` |
| 4 | `AsyncWebServerRequest` constructor is private in ESPAsyncWebServer 3.x | `Ipv6Network.cpp` (second IPv6 listener) | AsyncTCP 3.x listens dual-stack: drop the second listener; move the local-network peer check into a filter on the main server |
| 5 | `AsyncUDPPacket::remoteIPv6()` raw bytes gone | `Ipv6Network.cpp` (discovery peer check) | Read the v6 bytes from `IPAddress` |
| 6 | `sntp_stop` → `esp_sntp_stop` | `TimeManager.cpp` | Rename |
| 7 | `HTTPClient::getStreamPtr()` returns `NetworkClient*` | `OtaUpdater.cpp` | Type change |
| 8 | mbedTLS 3: `mbedtls_sha256_*_ret` removed | `LanguagePack.cpp` | Use `mbedtls_sha256_starts/update/finish` |
| 9 | `WiFi.enableIpV6()` → `WiFi.enableIPv6()`; IPv6 must be enabled before connecting | `WiFiManager.cpp` | Rename; move the call before `WiFi.begin` |
| 10 | `esp_task_wdt_init(timeout, panic)` → config struct; watchdog already initialised by the core | `main.cpp` | `esp_task_wdt_reconfigure(&config)` |
| 11 | C++20 (default `gnu++2b`): `++` on a `volatile` is deprecated | `WindSensor.cpp` | `x = x + 1` or `std::atomic` |
| 12–14 | NimBLE 2.x: `onConnect`/`onWrite` take `NimBLEConnInfo&`; `setScanResponse` → `enableScanResponse` | `BleService.cpp` | Signature updates |

`lib/` (pure logic) needed no changes; **all 247 native tests pass** under PlatformIO 6.2. Items
4–5 were stubbed in the spike and are the only real design work (IPv6 listener and discovery peer
check). Everything else is mechanical.

### Sizes on 3.x

| Build | 2.x today | 3.x as is | 3.x + size sdkconfig | 3.x + size sdkconfig + WPA3 off |
|---|---|---|---|---|
| Standard (slot 1,572,864) | 1,490,753 (94.8%) | **1,652,959 (105.1%)** – doesn't fit | **1,562,731 (99.4%)** – 10 KB free | 1,535,795 (97.6%) |
| BLE (slot 1,769,472) | 1,719,205 (97.2%) | not built (larger) | **1,762,443 (99.6%)** – 7 KB free | ~1,735,000 (98.1%)¹ |

¹ Measured on a build that accidentally had WPA3 off; see the note on sticky sdkconfig below.

Static RAM is about the same (standard 55.6 KB vs 55.8 KB; BLE 65.9 KB vs 65.1 KB).

**Where 3.x grows** (code, `.flash.text`, standard): +187 KB in total, of which the WiFi/TCP stack
(net80211 +30 KB, lwip +26 KB, wpa_supplicant +17 KB, pp +10 KB, phy +5 KB) is about +89 KB; the
new Arduino `Network` layer +17 KB; split-out IDF drivers (uart +14 KB, i2c +9 KB, gpio +6 KB);
`Hash` +10 KB; ESPAsyncWebServer +7 KB; AsyncTCP +6 KB; our own code +3–5 KB per library under
GCC 14. Read-only data shrinks by about 34 KB. Merged string literals alone are about 172 KB in the
untrimmed 3.x image, so log levels matter.

**Size sdkconfig** (pioarduino hybrid compile, `custom_sdkconfig`; it rebuilds ESP-IDF from source,
~2–3 min per env):

| Setting | Effect |
|---|---|
| `CONFIG_LOG_DEFAULT_LEVEL_ERROR`, `CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT` | IDF log strings above error removed |
| `CONFIG_ESP_ERR_TO_NAME_LOOKUP=n` | Error-name table removed (codes still logged) |
| `CONFIG_ESP_COREDUMP_ENABLE_TO_NONE` | No core dump (none is collected today) |
| `CONFIG_COMPILER_OPTIMIZATION_SIZE` | `-Os` for IDF |
| `CONFIG_BT_ENABLED=n` (standard only) | Bluetooth out of the standard image |
| `CONFIG_MBEDTLS_TLS_CLIENT_ONLY`, CAMELLIA/CCM/XTEA/DTLS off | TLS server code and unused ciphers out (the device is only a TLS client) |
| WPS registrar, DPP, 802.11k/v/r, NAN, mesh, FTM, GMAC, OWE off | Only ~3.5 KB: most of the WiFi stack is closed-source blobs |
| `CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=n` | **−27 KB, but devices can't join WPA3-only networks** (WPA2/WPA3 mixed networks still work). Owner's decision; left on. |
| `CONFIG_LIBC_NEWLIB_NANO_FORMAT=y` | Didn't link (`_printf_float` missing). Not pursued. |

BLE build adds `CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y` (no Bluetooth Classic controller).

Two pitfalls for the real migration:

- **The hybrid compile is sticky.** It overwrites the framework package in place and writes
  `sdkconfig.<env>`/`sdkconfig.defaults` into the project; removing an option does not restore its
  default. Builds must start from a clean package and clean generated files (CI does), and the
  generated files must be git-ignored.
- **Option names changed** in IDF 5.5 (e.g. `CONFIG_NEWLIB_NANO_FORMAT` →
  `CONFIG_LIBC_NEWLIB_NANO_FORMAT`). The build must verify each `custom_sdkconfig` line landed in
  the generated sdkconfig.

### Free space on the flash today

The standard layout (`partitions.csv`) ends LittleFS at 0x390000: **448 KB of the 4 MB flash
(0x390000–0x400000) is unused**. The BLE layout leaves 64 KB unused between app1 and LittleFS.
Since PR #114 stores the web UI gzipped, LittleFS needs about 268 KB at worst (UI + one 64 KB
language file + overhead), so 448 KB would be plenty.

A single layout for both builds that uses the whole chip:

| Partition | Offset | Size |
|---|---|---|
| nvs | 0x9000 | 0x5000 |
| otadata | 0xE000 | 0x2000 |
| app0 | 0x10000 | 0x1C0000 (1,835,008 B) |
| app1 | 0x1D0000 | 0x1C0000 |
| littlefs | 0x390000 | 0x70000 (448 KB) |

On it, 3.x + size sdkconfig would use **85.2%** (standard) and **96.0%** (BLE). Only by USB flash.

## R4. Upgrading deployed devices

Deployed bootloader: built by ESP-IDF **4.4.7** (Arduino 2.0.17), with app rollback enabled
(`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`); no secure boot or flash encryption.

- **Booting a 3.x app with the 4.4 bootloader.** ESP-IDF keeps app images bootable by older
  second-stage bootloaders on the ESP32 (only bootloaders older than v3.1 need special
  compatibility options), and OTA never rewrites the bootloader. The image header keeps the legacy
  chip-revision field the 4.4 bootloader reads. Expected to work; **must be proven on the spare.**
  Sources: ESP-IDF "Bootloader" and "OTA" API guides; ESP-IDF 5.0 migration guide (system/
  bootloader); pioarduino platform documentation.
- **Partition table.** Unchanged on option B, so OTA works as today. Any layout change (option C)
  needs a USB flash, because OTA can't rewrite the partition table.
- **NVS (settings).** The NVS format is unchanged between IDF 4.4 and 5.x; settings carry over.
- **LittleFS, the real risk.** 3.x uses a newer littlefs that can write on-disk version 2.1
  (`CONFIG_LITTLEFS_MULTIVERSION` exists and is off). The 2.x firmware's littlefs may refuse to
  mount a 2.1 filesystem, and the web UI lives on LittleFS. Two cases:
  - **Downgrading 3.x → 2.x** after the filesystem has been written could leave the old firmware
    unable to mount it (no web UI).
  - **Spec 012 writes the filesystem image before the firmware.** If a 2.x device downloads a 3.x
    release, writes a filesystem image built by the new `mklittlefs`, and the firmware step then
    fails, the 2.x firmware may not mount it.

  Mitigation: build the 3.x firmware with the 2.0 on-disk version (`CONFIG_LITTLEFS_MULTIVERSION=y`
  plus the 2.0 disk version option), build the filesystem image as 2.0, and for the transition
  release write the firmware before the filesystem. Verify all three on the spare.
- **Rollback.** With rollback enabled, a 3.x image that fails before marking itself valid falls
  back to the 2.x slot. One that starts and later misbehaves is kept, as today (spec 024 DIA-08).
  Manual way back: OTA-flash a 2.x release (works only if the LittleFS point above holds).

**Verification on the spare (later, by the implementation):**

1. Spare on beta.3/main (2.x); record config.
2. OTA the 3.x firmware, then the 3.x filesystem image; confirm boot, settings kept, language file
   kept, UI loads.
3. Kill power mid-firmware-write; confirm it boots the old slot.
4. OTA back to a 2.x release; confirm it mounts LittleFS and keeps settings.
5. Only then the main device (BLE build).

## R5. Options

| | What | Standard | BLE | Existing devices | Effort | Risk |
|---|---|---|---|---|---|---|
| **A** | Stay on 2.x; trims; CI budget | 94.4% | 96.6% | OTA as today | Small | Low, but 2.x is end-of-line; no IPv6 for TLS/NTP; ~50 KB left on BLE |
| **B** | 3.x with current partitions + size sdkconfig | 99.4% | 99.6% | OTA (after spare verification) | Medium (14 changes + IPv6 listener redesign + LittleFS version handling) | **No headroom**: the next feature fails the budget; WPA3-off (−27 KB) is the only big lever left |
| **C** | 3.x + one layout using the whole chip (app slots 1.75 MB, LittleFS 448 KB) | 85.2% | 96.0% | **One USB flash per device**, then OTA again | B + a layout migration and docs | Remote devices (the owner's main device) need hands-on once |
| **C′** | C for standard devices only; BLE keeps its layout | 85.2% | 99.6% | USB only for standard devices | as C | BLE stays tight |

**Recommendation**

1. **Ship beta.4 on 2.x** with the low-risk trims (`CORE_DEBUG_LEVEL=1`, NimBLE log off) and the
   CI flash budget. It is the stable fallback and the last 2.x release.
2. **Migrate to 3.x as option B first** (no partition change, OTA for everyone), with the LittleFS
   2.0 on-disk version and firmware-before-filesystem ordering for the transition release, proven
   on the spare.
3. **Plan option C** (one whole-chip layout) as a separate, announced step, since B leaves almost no
   room. Do it when a device can be reached by USB. The owner decides whether WPA3-only support is
   worth 27 KB before then.
4. Revisit when Arduino 4.0 (ESP-IDF 6.1) is final; it is likely larger, so the layout question
   doesn't go away.
