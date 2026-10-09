# Device results: Firmware platform and space

**Device**: the spare (standard build, MAC 0c:b8:15:77:b1:6c, 192.168.1.128), never the main device.
**Date**: 2026-10-09. **Firmware**: 0.3.0-beta.1+dev, layout l2, pioarduino 55.03.312-1
(Arduino-ESP32 3.3.12, ESP-IDF 5.5.5).

Before flashing: the whole flash was read back (4 MB) as a fallback, `/api/config` saved, and the
WiFi network confirmed in the saved settings.

## Results

| # | Check | Result |
|---|---|---|
| 1 | USB flash of the l2 package with esptool, no erase: bootloader 0x1000, partitions 0x8000, boot_app0 0xE000, firmware 0x10000, LittleFS 0x390000 | Pass - every part hash-verified; booted on layout l2 |
| 2 | Settings and WiFi kept across the move (FR-018) | Pass - joined the same network at the same address; `/api/config` identical to the saved copy |
| 3 | Every page and GET route | Pass - all pages 200; every GET API route answers |
| 4 | Contract check (`tools/contract-check.py`) | Pass - every endpoint and action in the documented shape |
| 5 | ConformU 4.5.0, SafetyMonitor and ObservingConditions, conformance and Alpaca protocol | Pass - no errors, warnings or issues (4 of 4) |
| 6 | Alpaca discovery | Pass (unicast); broadcast not reachable from the test machine's subnet |
| 7 | GitHub update check (FR-008) | Pass after two fixes found here (below): 8 of 8 checks in a row, then 3 of 3 straight after a restart, about 2.7 s each |
| 8 | TLS alert (ntfy.sh test notification) | Pass - delivered (HTTP 200) and received |
| 9 | NTP | Pass - clock set after boot; certificate checks need it |
| 10 | IPv6 | Not testable here: the spare's network has link-local and unique-local addresses only and the test machine has no IPv6 route to it. Covered by native tests (`lib/NetAddress`) |
| 11 | Language: upload a pack, served as `/lang.json`, back to English | Pass. Install from GitHub fails as expected: no v0.3.0-beta.1 release exists yet |
| 12 | HTTP OTA of later builds | Pass - several builds; "Could not activate partition" on some first attempts, the retry works (known before this spec) |
| 13 | Rollback (FR-021): OTA of a `SQM_TEST_BOOT_CRASH` build | Pass - the new slot aborted, otadata marked it ABORTED, and the device returned to the previous slot and started normally |
| 14 | Old-layout firmware upload (v0.2.0-beta.3) | Refused, 400 "Not firmware for this device. Use a v0.3 or later release file."; no restart |
| 15 | Wrong build (Bluetooth firmware on the standard device) | Refused, 400 "Wrong build: this device needs the standard build." |
| 16 | Old-layout web UI image (512 KB) by upload | Refused, 400 "Web UI file is for a different device layout..."; web UI kept |
| 17 | Update from GitHub with v0.2.0-beta.3 files | Refused before erasing ("Filesystem image is 524288 bytes; this partition is 458752"); web UI kept |
| 18 | Heap and stack (`/api/status`) | Idle free heap about 183-188 KB (largest block 110 KB), against 170 KB on 2.x; stack left: async_tcp about 6.8 KB, loop about 6.3 KB |
| 19 | Browser flasher (ESP Web Tools) | Not run by hand: it needs a person at a Web Serial browser. The docs page builds with the self-hosted flasher, and its manifest has the same five parts and offsets as the esptool command in check 1, with "erase" off by default |

## Found and fixed during these checks

- **Update check failed on 3.x.** `NetworkClient::readBytes()` gives up as soon as the TLS client
  has nothing decrypted yet, and ArduinoJson took that as the end of the list (`IncompleteInput`).
  The parse now reads through a reader that waits for data.
- **Low memory for TLS beside the parse buffer.** On 3.x, handshakes sometimes failed (allocation
  failures, certificate checks failing). `CONFIG_MBEDTLS_DYNAMIC_BUFFER` allocates TLS buffers
  only while they're in use, which raised idle free heap by about 40 KB.
- **The check blocked the web server's task.** A slow check could trip that task's watchdog and
  restart the device. The check now runs in its own task and answers the paused request; one retry
  covers a dropped connection; a second check while one runs gets 409.

## Settings changed for the checks and restored

Alpaca was switched on for ConformU, with an ntfy topic for the alert test and German for the
language check. Everything was put back afterwards, and `/api/config` matches the saved copy.
