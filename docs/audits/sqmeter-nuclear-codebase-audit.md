# SQMeter Nuclear Codebase Audit

**Date:** 2026-05-05
**Auditor:** Claude Sonnet 4.6 (automated deep inspection)
**Branch audited:** `feat/rg15-rain-sensor`
**Commit:** `0434605`
**Scope:** Full repository — firmware, UI, API, MQTT, CI/CD, docs, tests, security, build system

---

## Executive Summary

SQMeter is an ambitious, well-structured ESP32-based sky quality monitor with a genuinely impressive feature set for an alpha project. The architecture shows clear design intent: a clean `SensorBase` hierarchy, separation of firmware modules, a lightweight Preact UI served from LittleFS, and a working CI/CD pipeline that produces flash-ready release binaries. The addition of RG-15 rain sensor support is well-integrated.

However, several issues pose real reliability and safety risks before this device can be trusted in an observatory automation context:

### Top 10 Highest-Impact Issues

1. **[CRITICAL] Blocking `delay()` calls inside ESPAsyncWebServer async callbacks** — WiFi connect, OTA restart, and MQTT test all call `delay()` inside async request handlers. On ESPAsyncWebServer this runs on a FreeRTOS task distinct from `loop()`. Blocking it can starve the network stack and trigger the watchdog.
2. **[CRITICAL] Race condition: `createSensorDataJson()` accessed from both `loop()` task and async web server task** — sensor readings are modified in `loop()` and serialized in async callbacks with no synchronization. Can produce garbled JSON or crash.
3. **[CRITICAL] WiFi and MQTT passwords logged to serial in plaintext** — `Config::save()` logs the entire config JSON including credentials at INFO level to the serial port.
4. **[HIGH] MQTT client ID is the topic name** — `connect(config.topic.c_str())` uses the topic (`sqm/data`) as the MQTT client ID. Multiple devices or broker reconnects will collide and cause unpredictable disconnections.
5. **[HIGH] `WiFi.scanNetworks()` called in async handler** — Blocks the async task for seconds. Will starve WebSocket clients and may trigger the watchdog during a scan.
6. **[HIGH] Config stored as pretty-printed JSON in NVS** — `serializeJsonPretty` produces unnecessarily large JSON. NVS has a per-key size limit (~4000 bytes). As sensors are added, this limit will be exceeded and config saves will silently fail.
7. **[HIGH] ArduinoOTA has no password** — Explicitly commented out: `// No password - don't call setPassword() to disable authentication entirely`. Any device on the same network can push arbitrary firmware.
8. **[HIGH] TCPServer (ASCOM) not connected to RG-15 sensor** — `handleRainRate()` returns `"0.0"` with a `TODO` comment, but the RG-15 sensor exists and is running. Observatory clients using ASCOM rain rate will always see zero, potentially overriding a roof-close safety trigger.
9. **[HIGH] WebSocket broadcasts sensor data 5× more often than sensors update** — `WS_SENSOR_BROADCAST_INTERVAL_MS = 1000ms` but `sensor.readIntervalMs = 5000ms` by default. Clients receive four stale-data broadcasts for every fresh reading, with no indication that the data is stale.
10. **[HIGH] Settings `PUT` request never reaches firmware** — `Settings.tsx` sends `PUT /api/config` but the firmware only registers a handler for `POST /api/config`. Saves silently fail in any browser that correctly uses PUT.

### Top 10 Quickest Wins

1. Fix `saveConfig` in `Settings.tsx` to use `POST` (one character change — `PUT` → `POST`).
2. Replace `config.topic.c_str()` with a unique client ID (device MAC + name) in `MQTTClient::connect()`.
3. Remove the full config JSON from the INFO log line in `Config::save()` — keep byte count only.
4. Change `serializeJsonPretty` to `serializeJson` in `Config::toJson()`.
5. Redact passwords in `GET /api/config` response (return empty strings or `"***"` for password fields).
6. Set a minimum `minFreeHeap` metric and expose `ESP.getMinFreeHeap()` in the status endpoint.
7. Add the RG-15 sensor reference to `TCPServer::setSensorReferences()` and implement `handleRainRate()`.
8. Change WebSocket broadcast interval to match `sensor.readIntervalMs` (or add a `dataTimestamp` field so clients can detect stale data).
9. Move WiFi scan to use `WiFi.scanNetworks(true)` (async mode) with a polling approach rather than blocking.
10. Add `logLevel` to config and default it to `INFO` rather than `DEBUG` in production.

### Overall Risk Level: **High**

The project is not ready for observatory automation safety use. It has several confirmed bugs (PUT/POST mismatch, MQTT client ID, async blocking) and one critical safety gap (ASCOM rain rate returning 0 while RG-15 data exists). It is suitable as a monitoring/dashboard device but should not be trusted for roof-safety interlocks until Phase 0 issues are resolved.

---

## Scorecard

| Area | Score | Rationale |
|------|-------|-----------|
| Architecture | 7/10 | Clean module separation, good use of C++17, clear sensor hierarchy. WebServer is slightly overloaded. |
| Firmware reliability | 5/10 | Confirmed async-blocking bugs, race condition on sensor data, WDT risk during WiFi scan. |
| Sensor abstraction | 7/10 | `SensorBase` hierarchy is solid. Reading structs are typed. Optional sensor pattern is consistent. |
| API/backend | 5/10 | PUT/POST mismatch is a confirmed bug. Docs are wrong in two places. Passwords returned in GET. |
| WebSocket/live telemetry | 6/10 | Two WebSocket channels is a good design. Broadcasting stale data 4/5 times is a significant UX flaw. |
| MQTT/telemetry | 4/10 | Client ID bug, no LWT/availability topic, millis() timestamp, field naming diverges from REST. |
| Embedded UI | 7/10 | Clean Preact/Tailwind app, good mobile layout. Validation logic has a fragile divide-by-2 hack. |
| Configuration/settings | 6/10 | Good Zod validation on UI side. Pretty-print JSON in NVS is wasteful and will fail at scale. |
| Testing | 2/10 | Only Playwright screenshot tests with no content assertions. Zero firmware or API unit tests. |
| CI/CD | 7/10 | Solid build pipeline with artifacts, release automation, and docs deployment. Missing lint/typecheck gates. |
| Documentation | 6/10 | Good hardware docs for RG-15. REST API docs have wrong response shapes in two places. MQTT docs incomplete. |
| Security | 4/10 | Passwords logged in plaintext. Unauthenticated OTA. Credentials returned via API. Acknowledged but not mitigated. |
| Observability | 6/10 | Good status endpoint. Missing `minFreeHeap`, reset reason, boot count, per-subsystem last error. |
| Build/release process | 7/10 | Clean PlatformIO + Vite pipeline. ESPAsyncWebServer pinned to GitHub HEAD (no hash). |
| Developer experience | 7/10 | Good DEV_WORKFLOW.md, CONTRIBUTING.md. Vite proxy requires manual IP edit. |
| Maintainability | 6/10 | Consistent naming. A few clear code smells (fragile validation path key, pretty-print in NVS). |

---

## Findings by Severity

### Critical

---

#### AUDIT-001
**Severity:** Critical
**Area:** Firmware — WebServer
**Title:** Blocking `delay()` calls inside ESPAsyncWebServer async request callbacks

**Evidence:** `src/WebServer.cpp`
- Line 209: `delay(100)` inside `/api/wifi/connect` handler before WiFi connect attempt
- Line 213: `while (... attempts < 20) { delay(500); }` — 10 second busy-wait in async callback
- Line 265: `delay(1000)` before restart in OTA success handler
- Line 327: `delay(1000)` before restart in `/api/restart` handler
- `src/sensors/RG15Sensor.cpp` lines 36 and 71: `delay(200)` inside `begin()`, which is called from `reconfigure()` which is called from `saveConfigCallback()` which is called from the async config POST handler

**Why it matters:** ESPAsyncWebServer runs its callback handlers on a separate FreeRTOS task (the lwIP/AsyncTCP task). Calling `delay()` on that task starves the network stack, drops WebSocket packets for connected clients, and can trigger the 30-second watchdog. This is a documented anti-pattern for ESPAsyncWebServer — their documentation explicitly warns against blocking in callbacks. The WiFi connect handler has a 10-second worst-case delay.

**Recommended fix:**
- For restart: schedule via `esp_timer_create` with a 500ms delay firing `ESP.restart()`, return the response immediately.
- For WiFi connect: use `WiFi.scanNetworks(true)` async mode or move the connection attempt to a flag polled in `loop()`.
- For RG15 `delay()`: replace with a non-blocking startup sequence tracked by a timer.

**Effort:** M
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-002
**Severity:** Critical
**Area:** Firmware — Concurrency
**Title:** Race condition: sensor readings written in `loop()` task, read in async WebServer task

**Evidence:** `src/WebServer.cpp:658` — `createSensorDataJson()` accesses `tslSensor.getReading()`, `bmeSensor.getReading()`, etc. via const references. These sensor objects' readings are mutated in `src/main.cpp:335-340` inside `loop()`. ESPAsyncWebServer request handlers and WebSocket broadcasts run on a separate FreeRTOS task. There is no mutex, semaphore, or atomic access protecting these reads.

**Why it matters:** When the async task reads a sensor struct while `loop()` is writing to it, the result is undefined behavior: partial reads, torn floats, or crashes on multi-word fields. On Xtensa (ESP32) float reads are not atomic. The `TSL2591Reading` struct has `float lux`, `uint16_t visible`, `uint16_t infrared`, `uint32_t full` and `uint32_t timestamp` — six words written non-atomically from one task and read from another.

**Recommended fix:** Add a `SemaphoreHandle_t` mutex per sensor (or a single shared sensor-data mutex), take it before writing in `update()` and before reading in serialization. Alternatively, use double-buffering: keep a "committed" snapshot that is atomically swapped after each sensor poll.

**Effort:** M
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-003
**Severity:** Critical
**Area:** Security — Credentials
**Title:** WiFi and MQTT passwords logged to serial in plaintext at INFO level

**Evidence:** `src/Config.cpp:59`:
```cpp
Logger::info(TAG, "Config JSON to save (%d bytes): %s", json.length(), json.c_str());
```
`src/Config.cpp:38`:
```cpp
Logger::info(TAG, "Loaded config JSON (%d bytes): %s", jsonStr.length(), jsonStr.c_str());
```
The config JSON includes `wifi.password` and `mqtt.password` in full. These are logged at INFO level, which is the default log level at runtime.

**Why it matters:** Anyone with serial monitor access (e.g., at a star party, or remote log capture) can read the WiFi password and MQTT credentials of the site network. If MQTT is used with an internet-facing broker, this is a credential leak.

**Recommended fix:** Log byte count only: `Logger::info(TAG, "Config saved (%d bytes)", json.length());`. Never log the JSON content at INFO. A DEBUG-level log is acceptable only if the password fields are masked (e.g., `"password":"***"`).

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

### High

---

#### AUDIT-004
**Severity:** High
**Area:** API — Settings
**Title:** Settings page sends `PUT /api/config`; firmware only handles `POST /api/config`

**Evidence:** `web/src/components/Settings.tsx:148`:
```typescript
const response = await fetch('/api/config', {
  method: 'PUT',  // Using PUT for full resource replacement
```
`src/WebServer.cpp:147`:
```cpp
AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler(
    "/api/config",
    ...
```
`AsyncCallbackJsonWebHandler` only responds to `POST` by default (it does not accept `PUT`). The firmware's SPA fallback will return `index.html` for any unmatched route, so the UI receives a 200 OK with HTML instead of JSON, and the save operation silently appears to succeed before the JSON parse fails.

**Recommended fix:** Change `method: 'PUT'` to `method: 'POST'` in `Settings.tsx`. The comment about "full resource replacement" is incorrect — the firmware already does partial merging via default values in `fromJson()`.

**Effort:** S
**Owner:** UI
**Dependency:** None

---

#### AUDIT-005
**Severity:** High
**Area:** Firmware — MQTT
**Title:** MQTT client ID is set to the topic name, not a device identifier

**Evidence:** `src/MQTTClient.cpp:123-130`:
```cpp
connected = mqttClient->connect(
    config.topic.c_str(),   // This is "sqm/data", not a device ID
    config.username.c_str(),
    config.password.c_str());
```

**Why it matters:** The MQTT specification requires client IDs to be unique per broker. Using the topic name (`sqm/data`) means: (1) if two SQMeter devices publish to the same topic, they will knock each other off the broker repeatedly; (2) if the same device reconnects, it may conflict with its own lingering session; (3) brokers that validate client IDs may reject the connection because topic strings contain `/` which is invalid in client IDs on some brokers.

**Recommended fix:** Generate client ID from device MAC: `"SQM-" + WiFi.macAddress().substring(9)` (last 6 hex digits). Expose this in config as `mqtt.clientId` with a sane default.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-006
**Severity:** High
**Area:** Firmware — WebServer
**Title:** `WiFi.scanNetworks()` is synchronous and blocks the async request handler

**Evidence:** `src/WebServer.cpp:472`:
```cpp
int n = WiFi.scanNetworks();  // Blocking — can take 2-5 seconds
```
This is called inside `handleWiFiScan()`, which is registered as an ESPAsyncWebServer route handler.

**Why it matters:** Blocks the async task for up to 5 seconds. WebSocket clients will not receive updates. The 30-second watchdog countdown is a concern if called multiple times quickly (though unlikely in practice). The `/api/wifi/scan` endpoint is exposed to the UI with no rate-limiting.

**Recommended fix:** Use `WiFi.scanNetworks(true)` (async, returns immediately with `WIFI_SCAN_RUNNING`). Create a polling endpoint `/api/wifi/scan/status` that returns results when ready, or use a `millis()`-based polling approach in `handle()`.

**Effort:** M
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-007
**Severity:** High
**Area:** Firmware — Config
**Title:** Config NVS entry uses pretty-printed JSON, risking NVS size limit failure

**Evidence:** `src/Config.cpp:203`:
```cpp
serializeJsonPretty(doc, output);
```
The NVS `putString()` key size limit is 4000 bytes. A fully populated config in pretty-print format (all fields, plausible hostname/broker strings) easily exceeds 600 bytes. With more sensors added (e.g., wind sensor config) and longer string values, this will silently fail: `Preferences::putString()` returns 0 on overflow, and `Config::save()` reports an error, but the firmware continues running with the old NVS config.

**Recommended fix:** Change to `serializeJson(doc, output)`. Compact JSON reduces size by ~40%. Also validate the resulting string length before saving and log a clear error if it would exceed 3800 bytes (with 200 bytes headroom).

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-008
**Severity:** High
**Area:** Security — OTA
**Title:** ArduinoOTA has no password, allowing any LAN device to push arbitrary firmware

**Evidence:** `src/main.cpp:222`:
```cpp
ArduinoOTA.setHostname(config.wifi.hostname.c_str());
// No password - don't call setPassword() to disable authentication entirely
```

**Why it matters:** Any device on the same network segment that can reach the ESP32 on port 3232 (mDNS OTA port) can flash arbitrary firmware without any credential. This is particularly risky at shared observatories, star parties, or any network with untrusted users.

**Recommended fix:** Either set a password (`ArduinoOTA.setPassword(...)`) exposed via config, or disable ArduinoOTA entirely and rely solely on the web OTA endpoint (`/api/update`). If ArduinoOTA is kept for convenience, document the risk prominently and note that it only applies to LAN access.

**Effort:** S
**Owner:** Firmware / Security
**Dependency:** None

---

#### AUDIT-009
**Severity:** High
**Area:** Firmware — TCPServer (ASCOM)
**Title:** ASCOM rain rate command returns 0.0 even when RG-15 sensor is operational

**Evidence:** `src/TCPServer.cpp:249-252`:
```cpp
String TCPServer::handleRainRate()
{
    // TODO: Implement when rain sensor added
    return "0.0";
}
```
The RG-15 sensor is fully implemented and operational. `TCPServer::setSensorReferences()` does not accept an `RG15Sensor*` pointer (see `src/TCPServer.cpp:42`).

**Why it matters:** Observatory automation software (N.I.N.A., Voyager, Sequence Generator Pro) uses the ASCOM ObservingConditions interface to check rain rate before slewing or unparking. If rain rate is always reported as 0.0, a roof safety interlock based on this value will never trigger during actual rainfall. This is a safety issue for telescope equipment.

**Recommended fix:** Add `RG15Sensor *rg15Sensor` to `TCPServer`. Update `setSensorReferences()`. Implement `handleRainRate()` to return `rg15Reading.rInt` (or 0.0 if sensor not initialized/status not OK).

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-010
**Severity:** High
**Area:** WebSocket
**Title:** WebSocket broadcasts sensor data 5× more frequently than sensors are polled

**Evidence:**
- `include/WebServer.h:60`: `WS_SENSOR_BROADCAST_INTERVAL_MS = 1000`
- `src/Config.cpp:135`: `cfg.sensor.readIntervalMs = 5000`
- `src/WebServer.cpp:87-94`: broadcasts unconditionally every 1 second

**Why it matters:** With default settings, the WebSocket sends the same unchanged reading 4 times between each actual sensor poll. There is no timestamp in the WebSocket payload, so clients cannot detect staleness. This causes the UI to show values that look "live" but are actually up to 5 seconds old with no indication. For a sky quality device this matters — a cloud moving in or out mid-reading should be flagged, not hidden.

**Recommended fix:** Either (1) tie WS broadcast to the sensor update cycle (broadcast only when `lastSensorUpdate` changes), or (2) keep 1-second broadcast but add `"dataTimestamp": lastSensorUpdate` to the payload so the UI can display "data age". Option 2 is recommended for smoother UX.

**Effort:** S
**Owner:** Firmware / UI
**Dependency:** None

---

#### AUDIT-011
**Severity:** High
**Area:** Security — API
**Title:** WiFi and MQTT passwords returned in plaintext via `GET /api/config`

**Evidence:** `src/Config.cpp:158,168`:
```cpp
wifi["password"] = this->wifi.password;
mqtt["password"] = this->mqtt.password;
```
These are included in the full `Config::toJson()` output returned by `GET /api/config`.

**Why it matters:** Any device or script that can make an unauthenticated HTTP request to the ESP32 can retrieve the WiFi password of the site network and any MQTT credentials. For devices on a shared observatory network or a guest network, this is a credential exposure.

**Recommended fix:** Return password fields as empty strings or masked (`"***"`) in the API response. On settings save, accept empty password fields as "keep existing" rather than clearing. Store passwords separately in NVS from other config if possible.

**Effort:** M
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-012
**Severity:** High
**Area:** Firmware — Config
**Title:** `LittleFS.begin(true)` silently formats the filesystem on mount failure

**Evidence:** `src/main.cpp:185`:
```cpp
if (!LittleFS.begin(true))  // 'true' = format on fail
```

**Why it matters:** If the flash is corrupted (e.g., power loss during a previous update), this will silently erase all web UI files. The device will still boot but the web UI will be empty. Users will see a blank browser page with no error explanation. This is a silent data loss scenario.

**Recommended fix:** Change to `LittleFS.begin(false)` and handle the error explicitly — log a clear error message and either attempt recovery or display a serial diagnostic suggesting re-flash of the filesystem.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-013
**Severity:** High
**Area:** Firmware — TSL2591 Lux Calculation
**Title:** CPL formula has redundant factors that will silently produce wrong values if constants change

**Evidence:** `src/sensors/TSL2591Sensor.cpp:83`:
```cpp
float cpl = (100.0F * 600.0F / 100.0F) * 9876.0F / LUX_COEFF;
```
The `100.0 / 100.0` cancels to 1.0. The actual TSL2591 datasheet CPL formula is `CPL = (ATIME_ms × AGAIN) / DF`. As written, if someone changes the integration time constant (e.g., from 600ms to 300ms), only one of the `600.0` magic numbers would be updated, producing a 2× error in all lux calculations.

**Why it matters:** This is the core calculation used for all SQM, NELM, and Bortle outputs. A 2× lux error translates to approximately a 0.75 mag/arcsec² error in SQM — enough to shift Bortle classification by one class.

**Recommended fix:** Extract constants as named `constexpr`:
```cpp
constexpr float INTEGRATION_TIME_MS = 600.0F;
constexpr float GAIN_FACTOR = 9876.0F;
constexpr float LUX_COEFF = 408.0F;
float cpl = (INTEGRATION_TIME_MS * GAIN_FACTOR) / LUX_COEFF;
```
Add a comment citing the datasheet formula.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

### Medium

---

#### AUDIT-014
**Severity:** Medium
**Area:** Firmware — MQTT
**Title:** MQTT timestamp is `millis()` (uptime ms), not wall-clock time

**Evidence:** `src/MQTTClient.cpp:163`:
```cpp
doc["timestamp"] = millis();
```

**Why it matters:** Home Assistant, Grafana, and any time-series database expect Unix epoch timestamps. `millis()` resets on every reboot and has no relationship to real time. Historic data stitching, graphing, and alert rules based on this timestamp are broken.

**Recommended fix:** Use `time(nullptr)` to get Unix epoch seconds if NTP has synced. Fall back to millis() only if time is not synced, and add a `"timeValid": true/false` field so consumers know the difference.

**Effort:** S
**Owner:** Firmware
**Dependency:** TimeManager must be accessible from MQTTClient (currently it is not — pass via callback or reference)

---

#### AUDIT-015
**Severity:** Medium
**Area:** Firmware — MQTT
**Title:** No MQTT LWT (Last Will and Testament) / availability topic

**Evidence:** `src/MQTTClient.cpp` — no `setWill()` call before `connect()`.

**Why it matters:** When the device loses power or WiFi, MQTT subscribers have no way to know the device is offline. Home Assistant will continue showing the last known sensor values as if they are current. For safety-critical use (observatory roof), this means a sensor failure is indistinguishable from a "clear sky" reading.

**Recommended fix:**
```cpp
mqttClient->setWill(
    (config.topic + "/availability").c_str(),
    1, true, "offline");
// On successful connect:
mqttClient->publish((config.topic + "/availability").c_str(), "online", true);
```
This is standard Home Assistant MQTT device availability pattern.

**Effort:** S
**Owner:** Firmware
**Dependency:** Topic schema should be documented first

---

#### AUDIT-016
**Severity:** Medium
**Area:** API
**Title:** `GET /api/status` response shape contradicts documentation

**Evidence:** `docs/api/rest.md:18-25` shows:
```json
{ "uptime": 3600, "freeHeap": 210432, "rssi": -62, "ip": "192.168.1.42", "version": "0.0.1" }
```
Actual firmware response (`src/WebServer.cpp:745-923`) nests `rssi` and `ip` under `wifi`, `version` under `firmware`, and includes dozens of additional fields. The doc example is the stale prototype shape.

**Why it matters:** External integrators (Node-RED, Home Assistant MQTT, Python scripts) reading the docs will build against the wrong schema and silently get `undefined` for every field they try to access.

**Effort:** S
**Owner:** Docs
**Dependency:** None

---

#### AUDIT-017
**Severity:** Medium
**Area:** API
**Title:** `GET /api/wifi/scan` response shape contradicts documentation

**Evidence:** `docs/api/rest.md:140-143` shows:
```json
[{ "ssid": "MyNetwork", "rssi": -55, "secure": true }]
```
Actual response (`src/WebServer.cpp:480-487`):
```json
{ "networks": [{ "ssid": "...", "rssi": ..., "encryption": "secured" }] }
```
Field name mismatch: `"secure"` vs `"encryption"`, and the response is wrapped in an object, not a raw array. The UI correctly handles the actual format (`data.networks`), but the docs are wrong.

**Effort:** S
**Owner:** Docs
**Dependency:** None

---

#### AUDIT-018
**Severity:** Medium
**Area:** Firmware — Reliability
**Title:** `readLine()` in RG15Sensor spins without yielding during the 500ms response timeout

**Evidence:** `src/sensors/RG15Sensor.cpp:163-187`:
```cpp
const uint32_t deadline = millis() + RESPONSE_TIMEOUT_MS;
while (millis() < deadline) {
    while (serial->available()) { ... }
}
```
No `delay()`, `yield()`, or `vTaskDelay()` inside the outer loop. In polling mode this runs every `sensor.readIntervalMs`.

**Why it matters:** In polling mode (`mode == "polling"`), this busy-waits for up to 500ms every poll cycle, consuming 100% CPU during that window. This prevents `wifiManager->handle()`, `webServer->handle()`, and `mqttClient->handle()` from running during the poll, causing dropped WebSocket packets and MQTT disconnections during rain conditions (exactly when you want telemetry most).

**Recommended fix:** Add `vTaskDelay(1)` or `taskYIELD()` inside the outer while loop, or restructure to a non-blocking state machine.

**Effort:** M
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-019
**Severity:** Medium
**Area:** Firmware — Observability
**Title:** No minimum free heap tracking or heap fragmentation diagnostics

**Evidence:** `src/WebServer.cpp:758`: `doc["freeHeap"] = ESP.getFreeHeap();` — current free heap only. `ESP.getMinFreeHeap()` is not called.

**Why it matters:** On embedded devices, heap fragmentation is a primary cause of crashes. The current free heap can look healthy while the minimum ever recorded is near zero (indicating past pressure or fragmentation). Without `minFreeHeap`, it is impossible to diagnose heap-related crashes in the field.

**Recommended fix:** Add to status payload:
```cpp
doc["minFreeHeap"] = ESP.getMinFreeHeap();
doc["maxAllocHeap"] = ESP.getMaxAllocHeap();
```

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-020
**Severity:** Medium
**Area:** Firmware — Reliability
**Title:** No reset reason or boot count in status or logs

**Evidence:** No calls to `esp_reset_reason()` or `esp_restart_reason()` anywhere in the codebase.

**Why it matters:** When a device reboots unexpectedly (watchdog, panic, power loss), there is no way to know why. Reset reason is essential for diagnosing firmware bugs in the field. A device that regularly reboots due to a watchdog triggered by the blocking async callback bug (AUDIT-001) will give no indication of this in the status endpoint.

**Recommended fix:**
```cpp
#include <esp_system.h>
Logger::info("Main", "Reset reason: %d", esp_reset_reason());
doc["resetReason"] = esp_reset_reason();
doc["bootCount"] = bootCount; // stored in RTC memory: RTC_DATA_ATTR int bootCount = 0;
```

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-021
**Severity:** Medium
**Area:** UI — Validation
**Title:** Validation error count calculation is a fragile hack

**Evidence:** `web/src/components/Settings.tsx:139`:
```cpp
const errorCount = Object.keys(validationErrors).length / 2;
```
The code stores each error under two keys (dotted path and flattened path), then divides by 2 to get the real count. This will produce incorrect results for:
- Nested errors where the dotted and flattened paths happen to be the same (e.g., `rain.enabled` → `rainEnabled` vs top-level fields)
- If the deduplication logic changes
- Any error path that doesn't produce exactly two keys

**Recommended fix:** Store errors in a `Map<string, string>` keyed by canonical dotted path only. Display using the dotted path with the nested section label. Remove the parallel flattened-key storage.

**Effort:** S
**Owner:** UI
**Dependency:** None

---

#### AUDIT-022
**Severity:** Medium
**Area:** UI — Mock data
**Title:** Mock config response uses `{ok: true}` but firmware returns `{success: true}`

**Evidence:** `web/src/mocks/handlers.ts:29`:
```typescript
http.post("/api/config", () => HttpResponse.json({ ok: true })),
```
`src/WebServer.cpp:162`:
```cpp
request->send(200, "application/json", "{\"success\":true}");
```
The mock returns `ok` but the real API returns `success`. The Settings component checks `response.ok` (the HTTP status, not the body), so this doesn't currently cause a UI bug, but any code that checks `result.ok` vs `result.success` will diverge between demo and real device.

**Effort:** S
**Owner:** UI/Mocks
**Dependency:** None

---

#### AUDIT-023
**Severity:** Medium
**Area:** Documentation
**Title:** CHANGELOG says GPS is "planned" but GPS is fully implemented

**Evidence:** `CHANGELOG.md:8`:
```markdown
- GPS time source implementation
```
Listed under `[Unreleased]`. But `src/sensors/GPSSensor.cpp`, `include/sensors/GPSSensor.h`, and the full GPS pipeline are implemented and functional.

**Why it matters:** Prospective users reading the changelog will think GPS is not yet available and may not set it up or expect it to work.

**Effort:** S
**Owner:** Docs
**Dependency:** None

---

#### AUDIT-024
**Severity:** Medium
**Area:** Firmware — Cloud Detection
**Title:** Cloud detection uses a fixed 53% humidity fallback that silently masks BME280 failures

**Evidence:** `src/WebServer.cpp:697`:
```cpp
float humidity = (bmeReading.status == SensorStatus::OK) ? bmeReading.humidity : 53.0f;
```
`53%` is an arbitrary default. When the BME280 fails, cloud detection continues using this fixed humidity. The API response includes `"humidityUsed": 53.0` but the `cloudConditions` object has no field indicating that the BME280 was unavailable. The MQTT payload (`src/MQTTClient.cpp:196-204`) skips cloud data when MLX is unavailable but does not similarly skip when BME is unavailable.

**Why it matters:** If the BME280 fails in a high-humidity environment (e.g., 85% actual humidity), the cloud cover estimate will be systematically wrong because the correction factor applied will be too small. Observatory users relying on cloud cover for go/no-go decisions will get incorrect results with no visible indication of the sensor failure.

**Recommended fix:** Add a `"humiditySource"` field to `cloudConditions` with values `"bme280"` or `"default"`. Consider adding a separate `"bme280Available"` flag. Log a warning when the fallback is used.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-025
**Severity:** Medium
**Area:** UI — Dashboard
**Title:** Rain sensor unit label is hardcoded as `mm/hr` regardless of configured units

**Evidence:** `web/src/components/Dashboard.tsx:184`:
```tsx
{sensors.rainSensor.rInt.toFixed(1)} mm/hr
```
If the sensor is configured for imperial units, the value will be in `in/hr` but the label still says `mm/hr`.

**Effort:** S
**Owner:** UI
**Dependency:** Config must be available to Dashboard (currently it is not — Dashboard only uses sensor data)

---

#### AUDIT-026
**Severity:** Medium
**Area:** CI/CD
**Title:** Screenshots Playwright test has `continue-on-error: true`, silently hiding failures

**Evidence:** `.github/workflows/docs.yml:66`:
```yaml
- name: Take screenshots
  run: npm run screenshots
  working-directory: web
  continue-on-error: true
```

**Why it matters:** If the demo app crashes, screenshots fail silently and the docs deployment proceeds with outdated or missing screenshots. This is the only UI regression check in CI and it's disabled.

**Recommended fix:** Remove `continue-on-error: true`. Fix any screenshot test flakiness at the root (the `waitForTimeout` anti-pattern) rather than suppressing errors.

**Effort:** S
**Owner:** CI/CD
**Dependency:** Playwright test stability (see testing gaps)

---

#### AUDIT-027
**Severity:** Medium
**Area:** CI/CD
**Title:** ESPAsyncWebServer and AsyncTCP pinned to GitHub HEAD with no hash

**Evidence:** `platformio.ini:33-34`:
```ini
https://github.com/me-no-dev/ESPAsyncWebServer.git
https://github.com/me-no-dev/AsyncTCP.git
```
No commit hash, no version tag. These are resolved at build time to whatever HEAD is.

**Why it matters:** A breaking change or incompatible commit pushed to these repos will break the firmware build without any change to the SQMeter codebase. This has happened historically with ESPAsyncWebServer. The build is not reproducible.

**Recommended fix:** Pin to a specific commit hash: `https://github.com/me-no-dev/ESPAsyncWebServer.git#<commit-sha>`. Alternatively migrate to the maintained fork `mathieucarbou/ESPAsyncWebServer` which has proper versioned releases.

**Effort:** S
**Owner:** Build
**Dependency:** Verify compatible commit

---

#### AUDIT-028
**Severity:** Medium
**Area:** CI/CD
**Title:** npm dependency cache keyed on `package.json`, not `package-lock.json`

**Evidence:** `.github/workflows/build.yml:65`:
```yaml
key: ${{ runner.os }}-npm-${{ hashFiles('web/package.json') }}
```
`package-lock.json` contains the exact resolved versions. Using `package.json` means the cache is not invalidated when transitive dependencies change.

**Effort:** S
**Owner:** CI/CD
**Dependency:** None

---

### Low

---

#### AUDIT-029
**Severity:** Low
**Area:** Firmware — Logger
**Title:** Logger defaults to DEBUG level in production, spamming serial at ~600ms intervals

**Evidence:** `src/Logger.cpp:12`:
```cpp
LogLevel Logger::currentLevel = LogLevel::DEBUG;
```
TSL2591 logs every read at DEBUG level (`src/sensors/TSL2591Sensor.cpp:132`). RG15 logs every parse. This generates significant serial output and introduces non-trivial delays if a serial monitor is attached, which may affect timing-sensitive operations.

**Recommended fix:** Default to `INFO` unless `DEBUG_BUILD` is defined. Add `logLevel` to `Config` so it can be changed at runtime.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-030
**Severity:** Low
**Area:** UI — WebSocket
**Title:** `useWebSocket` reconnect uses a fixed 5-second delay with no exponential backoff

**Evidence:** `web/src/hooks/useWebSocket.ts:38`:
```typescript
setTimeout(connect, 5000);
```

**Why it matters:** If the device is rebooting after an OTA update (which takes 15-30 seconds), the client will make ~3-6 failed connection attempts before the device is ready. While not harmful, it creates unnecessary log noise and could stress a slow device during startup.

**Recommended fix:** Implement exponential backoff with jitter: `setTimeout(connect, Math.min(30000, 5000 * 2**retries + Math.random()*1000))`.

**Effort:** S
**Owner:** UI
**Dependency:** None

---

#### AUDIT-031
**Severity:** Low
**Area:** Firmware — TCPServer
**Title:** TCPServer uses `new WiFiServer(port)` without `delete` on reconfiguration

**Evidence:** `src/TCPServer.cpp:14-37`: `server = new WiFiServer(port)` allocated in constructor. Destructor deletes it. However, if `begin()` is called after a reconnect (it checks `WiFi.isConnected()` but doesn't retry on connect), the server is never started.

**Why it matters:** Minor: if WiFi disconnects and reconnects, the TCP server is never restarted. ASCOM clients won't be able to reconnect.

**Recommended fix:** Check server state in `handle()` and call `begin()` if WiFi is connected but server is not running.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

#### AUDIT-032
**Severity:** Low
**Area:** Firmware — GPS
**Title:** GPS data appears in two separate places in both API and UI with slightly different structure

**Evidence:**
- In `GET /api/sensors`: GPS data under `"gps"` key, `hdop` as raw value
- In `GET /api/status` (via `gpsData`): GPS data under `"gpsData"` key, `hdop` divided by 100 (`gpsData["hdop"] = gpsReading.hdop / 100.0`)
- Dashboard shows GPS from sensor endpoint; System page shows GPS from status endpoint

This means HDOP is inconsistently scaled between the two endpoints. `api/sensors` shows raw HDOP (e.g., 110), `api/status` shows divided HDOP (e.g., 1.10). Both appear in the UI in different pages.

**Recommended fix:** Normalize HDOP to actual floating-point value (divide by 100) at the reading source, not at serialization time.

**Effort:** S
**Owner:** Firmware
**Dependency:** None

---

### Positive Findings

---

#### AUDIT-P01
**Severity:** Positive
**Area:** Architecture
**Title:** Clean `SensorBase` polymorphic hierarchy with consistent optional-sensor handling

Each sensor implements `begin()`, `update()`, `getName()`, `toJson()`. Optional sensors (BME280, MLX, GPS, RG15) are created unconditionally and their `isInitialized()` flag gates all usage. This is an excellent pattern that makes adding new sensors straightforward.

---

#### AUDIT-P02
**Severity:** Positive
**Area:** Build
**Title:** Complete flash image artifact produced in CI with correct partition layout

The CI workflow produces `complete-flash.bin` by merging bootloader, partition table, `boot_app0.bin`, firmware, and LittleFS — excluding NVS so user config is preserved. This is exactly what first-time flashers need and is often done incorrectly.

---

#### AUDIT-P03
**Severity:** Positive
**Area:** Firmware
**Title:** Watchdog timer configured and fed in `loop()`

30-second watchdog with `esp_task_wdt_reset()` in `loop()` provides a last-resort safety net. This is correctly implemented.

---

#### AUDIT-P04
**Severity:** Positive
**Area:** UI
**Title:** Zod schema validation on settings form with inline error display

`web/src/validation/configSchema.ts` provides thorough Zod validation with GPIO pin allow-listing, cross-field validation (SDA ≠ SCL, RX ≠ TX when enabled), and clear error messages. This is well above average for an embedded device UI.

---

#### AUDIT-P05
**Severity:** Positive
**Area:** CI/CD
**Title:** Separate build and release jobs with controlled artifact retention

PR builds retain artifacts 5 days; tag builds retain 90 days. Release job only runs on tags. Concurrency group correctly cancels superseded PR builds.

---

#### AUDIT-P06
**Severity:** Positive
**Area:** Firmware
**Title:** NVS config verification after save

`src/Config.cpp:80-87` reads back the NVS value immediately after writing to verify it was saved correctly. This is a good practice for flash-backed storage.

---

#### AUDIT-P07
**Severity:** Positive
**Area:** Firmware
**Title:** Exponential backoff on WiFi reconnect

`src/WiFiManager.cpp:150-154`: `currentReconnectDelay = std::min(currentReconnectDelay * 2, config.maxReconnectDelayMs)`. Correctly implemented with a configurable cap.

---

#### AUDIT-P08
**Severity:** Positive
**Area:** Documentation
**Title:** RG-15 wiring and protocol documentation is excellent

`docs/hardware/rg15.md` includes pin-by-pin wiring table, correct TX/RX cross-wiring note, DIP switch vs serial priority explanation, troubleshooting table, and MQTT/API payload examples. This is production-quality hardware documentation.

---

## Code Smells

| File/Path | Smell | Impact | Fix |
|-----------|-------|--------|-----|
| `src/WebServer.cpp:259-263` | Heap-allocates `String *responseStr = new String()` then immediately `delete`s it after use | Memory fragmentation, unnecessary allocation | Use stack `String responseStr;` |
| `src/WebServer.cpp:354-441` | Static local variables for filesystem OTA state (`fs_partition`, `fs_bytes_written`, etc.) | Not thread-safe, state persists across requests | Move to class members or use a dedicated state struct |
| `src/Config.cpp:59` | Full config JSON logged at INFO level including passwords | Security leak | Log byte count only |
| `src/Config.cpp:203` | `serializeJsonPretty` for NVS storage | Wastes NVS space | Use `serializeJson` |
| `src/MQTTClient.cpp:124` | Topic used as MQTT client ID | Protocol violation, device conflicts | Use MAC-based client ID |
| `src/sensors/TSL2591Sensor.cpp:83` | `(100.0F * 600.0F / 100.0F)` — cancelling constants in formula | Confusing, error-prone on maintenance | Extract named constants |
| `web/src/components/Settings.tsx:139` | `/ 2` hack for validation error count | Fragile, breaks with any path deduplication change | Use Map with canonical keys |
| `web/src/components/Settings.tsx:168` | `updateConfig` uses shallow spread then mutates deep path | Could cause missed Preact re-renders | Use proper immutable deep update |
| `web/src/hooks/useWebSocket.ts:34-40` | Creates `ws` state but cleanup effect uses stale closure reference | WebSocket may not be closed on unmount | Use ref for socket, not state |
| `include/WebServer.h:60-61` | `WS_SENSOR_BROADCAST_INTERVAL_MS` and `WS_STATUS_BROADCAST_INTERVAL_MS` don't reference `config.sensor.readIntervalMs` | Hardcoded coupling | Derive from config |
| `src/TCPServer.cpp` | Entire file uses `raw pointer new` for `WiFiServer`, `Arduino String` everywhere else uses `std::string` | Inconsistent ownership model | Use `std::unique_ptr<WiFiServer>` |
| `src/WebServer.cpp:136-146` | `GET /api/status` and `POST /api/config` registered separately but no verb guard on GET config | Potential confusion | Explicit verb routing |
| `web/src/mocks/data.ts:70` | Mock firmware build date hardcoded: `"Apr 24 2026"` | Will drift from reality | Use `new Date().toLocaleDateString()` |
| `web/src/mocks/handlers.ts:29` | Config save mock returns `{ok: true}` but firmware returns `{success: true}` | Contract drift | Align response shape |
| `src/WebServer.cpp:430` | `if (index % 10240 == 0)` log in filesystem OTA — `index` is always 0 on first chunk | Logging never fires as intended | Use `fs_bytes_written` for progress logging |

---

## Testing Gaps

| Area | Current Coverage | Missing Tests | Priority | Suggested Test Files |
|------|-----------------|--------------|---------|---------------------|
| Firmware — SkyQuality | None | SQM→Bortle boundary values, lux=0, lux=100000, NaN handling | Critical | `test/test_sky_quality.cpp` |
| Firmware — CloudDetection | None | Humidity correction range, threshold boundaries, MLX unavailable fallback | Critical | `test/test_cloud_detection.cpp` |
| Firmware — RG15 parser | None | Happy path, too-short line, partial parse, flag characters, imperial line | High | `test/test_rg15_parser.cpp` |
| Firmware — Config | None | Round-trip toJson/fromJson, partial JSON with missing keys, oversized JSON | High | `test/test_config.cpp` |
| Firmware — TSL2591 lux | None | Saturation path, IR > full path, normal path | High | `test/test_tsl2591.cpp` |
| API — contract | None | Response shape for all endpoints, error response shapes | High | `web/tests/api.spec.ts` |
| UI — Settings | None | Save with valid data, save with validation errors, PUT vs POST regression | High | `web/tests/settings.spec.ts` |
| MSW — contract | None | Mock payloads match TypeScript types | Medium | `web/tests/mock-contract.test.ts` |
| Firmware — MQTT payload | None | Timestamp field is epoch not millis, client ID is device ID not topic | Medium | `test/test_mqtt_payload.cpp` |
| UI — WebSocket | None | Reconnect on close, data staleness indicator | Medium | `web/tests/websocket.spec.ts` |
| Playwright — content | Screenshot-only (no assertions) | Assert specific SQM value visible, sensor status badges, error states | High | Extend `tests/screenshots.spec.ts` |
| Firmware — hardware abstraction | None | No mock/stub layer for I2C sensors | Medium | Requires hardware abstraction layer first |

---

## CI/CD Gaps

| Workflow/File | Current Behaviour | Gap | Recommended Change | Priority |
|--------------|-------------------|-----|-------------------|---------|
| `build.yml` | Runs `npm run build` (which runs `tsc && vite build`) | TypeScript errors only fail if Vite build fails — `tsc --noEmit` errors are not surfaced as a separate step | Add `- run: npx tsc --noEmit` before Vite build | High |
| `build.yml` | No lint step | ESLint is in `package.json` scripts but never called in CI | Add `- run: npm run lint` | Medium |
| `docs.yml` | Screenshots `continue-on-error: true` | Screenshot failures are silently ignored | Remove `continue-on-error` after fixing Playwright `waitForTimeout` | Medium |
| Both workflows | npm cache on `package.json` hash | Transitive dep changes not caught | Change to `hashFiles('web/package-lock.json')` | Medium |
| Both workflows | ESPAsyncWebServer pinned to HEAD | Not reproducible | Pin to specific commit hash | High |
| `build.yml` | No firmware size tracking | Binary grows silently | Add step to `cat .pio/build/esp32dev/firmware.map \| grep -E 'FLASH\|RAM'` and fail if > 90% | Medium |
| Neither | No CHANGELOG.md check | Tags can be released without changelog entry | Add a check that `CHANGELOG.md` has an entry for the tag version | Low |
| Neither | No dependabot/Renovate | JS deps go stale silently | Add Dependabot config for npm and GitHub Actions | Low |

---

## Documentation Audit

### Missing Documentation

| Missing Doc | Impact | Priority |
|-------------|--------|---------|
| ASCOM ObservingConditions driver — what commands are supported, port, protocol | Observatory integration users | High |
| Calibration — is the TSL2591 SQM calibrated against reference? What is the offset/accuracy? | Scientific users | High |
| Supported ESP32 boards — only says "ESP32" but GPIO defaults only work for specific variants | Reproduction | High |
| Config schema reference — complete field-by-field table of all config keys and defaults | Setup | High |
| OTA update docs incomplete — no `api/update/fs` endpoint documented | Users with UI issues | Medium |
| Node-RED flow examples | Integration | Medium |
| Reset config / factory reset procedure (not just "erase_flash") | Troubleshooting | Medium |
| Network security recommendations — when to use reverse proxy | Security | Medium |
| N.I.N.A. / Voyager ASCOM setup | Observatory use | High |
| Sensor accuracy and known limitations table | Scientific use | Medium |

### Stale Documentation

| Doc | Stale Content | Fix |
|-----|--------------|-----|
| `CHANGELOG.md:8` | GPS listed as planned/unreleased | Move to released section |
| `docs/api/rest.md` | `/api/status` response shows flat structure | Update to actual nested structure |
| `docs/api/rest.md` | `/api/wifi/scan` shows array, `"secure"` field | Update to `{networks: [...]}` with `"encryption"` field |
| `docs/api/websocket.md` | Missing `rainSensor` in message format | Add `rainSensor` section |
| `docs/user-guide/mqtt.md` | Payload missing `infrared`, `cloudConditions`, `rain`, `location` objects | Update to full payload example |
| `docs/user-guide/mqtt.md` | Home Assistant YAML uses `environment.temperature` but IR temp sensor is `infrared.skyTemp` | Fix field paths |

### Contradictions Table

| Doc | Code | Contradiction |
|-----|------|--------------|
| `docs/api/rest.md` shows `status.condition = 0` for Clear | `src/calculations/CloudDetection.h`: `CLEAR=?`, but enum starts from unnamed 0 | Docs show 0=Clear, code has `CloudCondition::CLEAR`, but `SensorBase.h` enum starts at 0=OK — risk of confusion between sensor status 0 and cloud condition 0 |
| `docs/api/rest.md` shows `"version": "0.0.1"` in status | Firmware returns `firmware.version` nested | Shape mismatch |
| `mockConfig` in `data.ts` has `ntp.daylightOffsetSec: 3600` | Default config has `daylightOffsetSec: 0` | Demo shows DST configured but default doesn't |
| `mockConfig.mqtt.topic = "sqmeter/data"` | Default config has `topic = "sqm/data"` | Demo topic diverges from default |
| `docs/hardware/rg15.md`: "default: GPIO 18" for RX | `Config.cpp:128`: `cfg.rain.rxPin = 18` (RX pin) and `cfg.rain.txPin = 19` | Doc says GPIO 18 is RX; code matches; consistent |

### Recommended Documentation Structure

The current structure is good. Suggested additions:
```
docs/
├── api/
│   ├── rest.md           ← update response shapes
│   ├── websocket.md      ← add rainSensor
│   └── ascom.md          ← NEW: TCP protocol, commands, port 2020
├── integration/          ← NEW directory
│   ├── home-assistant.md ← move from mqtt.md, expand
│   ├── nina.md           ← NEW: N.I.N.A. setup
│   └── node-red.md       ← NEW: Node-RED flow
└── reference/
    ├── config-schema.md  ← NEW: all config fields, types, defaults
    └── calibration.md    ← NEW: SQM accuracy, offsets
```

---

## API/Telemetry Contract Audit

### Endpoint Summary

| Endpoint | Method | Returns | Issues |
|---------|--------|---------|--------|
| `/api/status` | GET | Nested status object | Docs wrong; missing `minFreeHeap`, `resetReason` |
| `/api/sensors` | GET | Sensor readings | Correct; missing data age/timestamp |
| `/api/config` | GET | Full config | Returns passwords in plaintext |
| `/api/config` | POST | `{success:true}` or error | UI sends PUT, firmware only handles POST |
| `/api/wifi/scan` | GET | `{networks:[...]}` | Blocks async thread; docs show wrong shape |
| `/api/wifi/connect` | POST | `{success:true, ip:...}` | Blocks async thread for 10s |
| `/api/mqtt/test` | POST | `{success:bool, message/error}` | Good; no blocking issues found |
| `/api/restart` | POST | `{success:true}` | delay(1000) in async handler |
| `/api/update` | POST | `{success:bool}` | Unauthenticated; correct otherwise |
| `/api/update/fs` | POST | `{success:bool}` | Not documented; unauthenticated |

### Field Name Inconsistencies: API vs MQTT vs WebSocket

| Concept | REST `/api/sensors` | WebSocket `/ws/sensors` | MQTT payload |
|---------|--------------------|-----------------------|-------------|
| Light data object | `lightSensor` | `lightSensor` | `light` |
| Sky quality object | `skyQuality` | `skyQuality` | `sky` |
| IR cloud | `cloudConditions` | `cloudConditions` | `clouds` |
| IR temperature | `irTemperature` | `irTemperature` | `infrared` |
| Rain sensor | `rainSensor` | `rainSensor` | `rain` |
| GPS data | `gps` | `gps` | `location` |

Every subsystem uses different field names for the same data. Any integration that uses more than one channel must handle two schemas.

### Recommended Stable Schema

```json
{
  "device": { "name": "...", "firmware": "...", "timestamp": 1700000000, "timeValid": true },
  "light": { "lux": 0.0234, "visible": 123, "infrared": 45, "full": 168, "status": 0 },
  "sky": { "sqm": 21.5, "nelm": 6.2, "bortle": 2, "description": "..." },
  "environment": { "temperatureC": 12.4, "humidity": 72.1, "pressureHPa": 1013.25, "dewpointC": 7.8, "status": 0 },
  "cloud": { "skyTempC": -15.2, "ambientTempC": 12.4, "deltaC": -27.6, "correctedDeltaC": -24.1, "coverPercent": 5.0, "condition": "clear", "humiditySource": "bme280", "status": 0 },
  "rain": { "isRaining": false, "accMm": 0.0, "eventAccMm": 0.0, "totalAccMm": 12.34, "intensityMmPerHr": 0.0, "lensBad": false, "emSat": false, "status": 0 },
  "gps": { "hasFix": true, "satellites": 8, "latitude": 51.5074, "longitude": -0.1278, "altitudeM": 42.0, "hdop": 1.2, "ageMs": 800 }
}
```

Adopt this as the single canonical schema for REST, WebSocket, and MQTT, with MQTT using the full object as the payload.

---

## UI/UX Audit

### Dashboard Issues
- Rain sensor shows hardcoded `mm/hr` label regardless of configured units (AUDIT-025)
- No data-age indicator — values look live but can be 5+ seconds old
- Cloud conditions card shows emoji icons that render as emoji in some terminals/screenshot tools but poorly on low-DPI displays
- Bortle `toFixed(1)` on a value that is always an integer (1.0, 2.0...) shows `1.0` not `1` — minor visual noise
- No fallback if the light sensor has READ_ERROR status — `sensors.lightSensor.lux.toFixed(6)` throws if lux is null

### Settings Issues
- GPS pin inputs use `<input type="number">` with `min=0 max=39` but no GPIO allow-list validation (unlike the Zod schema which does check this)
- No indication that some settings require reboot to take effect (I2C pins, GPS pins — these are applied in `setup()` only)
- Validation error count divides by 2 (AUDIT-021)
- `updateConfig(['rain', 'enabled'], ...)` mutates a deep path via a shared reference, which may not trigger Preact's shallow re-render

### Mobile/Tablet
- Dashboard is well-responsive with `grid grid-cols-1 md:grid-cols-2`
- Settings page scrolls to validation errors on mobile — good
- No PWA manifest or service worker for home screen install (separate from MSW)
- Touch targets on toggle checkboxes are small (4×4 block = 16px) — below WCAG AA 44px minimum

### Accessibility
- No `aria-label` on icon-only elements (emoji icons as status indicators)
- Color-only status indicators (green/red dots) without text for colorblind users
- No `<label for>` association on some checkbox fields — screen readers may not read the label

### Bundle Size (Embedded)
- `data/assets/index-z9OjbY1v.js`: **140 KB** (minified, presumably before gzip)
- `data/assets/index-D_kLi0L7.css`: **17 KB**
- LittleFS partition: 512 KB total, 40 KB in use (per mock data — actual TBD)
- Preact is ~3 KB gzipped, Tailwind (purged) is typically 5-15 KB, Zod ~12 KB gzipped
- **Assessment:** The 140 KB JS bundle is large for ESP32 serving but LittleFS has ~470 KB free. The main risk is gzip decompression time on client. The vite config correctly uses terser. Consider adding `build.reportCompressedSize: true` to track gzip size.

---

## Security Audit

### Realistic LAN-Device Threat Model

SQMeter is designed as a LAN-only device, which limits the attack surface significantly. The realistic threat model is:

1. **Local network attacker** — any device on the same WiFi/VLAN can reach port 80 and port 2020 without authentication
2. **Shared observatory network** — star parties, club observatories, or guest WiFi users can reach the device
3. **Compromised device on LAN** — malware on a laptop can reach and reconfigure the device
4. **Physical access** — serial port reveals all credentials (see AUDIT-003)

### Security Findings

| Risk | Severity | Finding |
|------|----------|---------|
| ArduinoOTA unauthenticated on port 3232 | High | Any LAN device can push arbitrary firmware. Acknowledged in SECURITY.md but not mitigated. |
| WiFi password exposed via `GET /api/config` | High | Retrieves the site WiFi password without authentication |
| MQTT password exposed via `GET /api/config` | High | Same as above |
| WiFi password logged to serial | Critical | Full config including password logged at INFO level |
| No input size limit on POST body | Medium | AsyncCallbackJsonWebHandler default max size is ~16 KB; malformed large requests can cause heap issues |
| No rate limiting on settings endpoints | Low | Repeated POST /api/config calls are accepted without limit |
| Config can be replaced entirely via POST | Medium | No authentication prevents a LAN attacker from changing WiFi credentials, MQTT, pins |
| Rain sensor always returns 0 via ASCOM | High | Observatory safety software gets wrong data (safety impact, not security) |

### What is Acceptable for a LAN Hobby Device

- No HTTPS — acceptable for LAN, but document that credentials travel unencrypted
- No UI authentication — acceptable if LAN is trusted; document the assumption
- Open OTA endpoint — **not** acceptable even for hobby use; the trivial fix (set a password) should be done

### Recommended Mitigations (Prioritized)

1. **Immediate:** Set ArduinoOTA password (exposes via config or hardcoded)
2. **Short-term:** Mask passwords in `GET /api/config` response
3. **Short-term:** Remove credential logging from `Config::save()`
4. **Medium-term:** Add optional HTTP Basic Auth to the web server (single username/password in config)
5. **Nice-to-have:** Add CSRF token for config-mutating endpoints (POST /api/config, POST /api/restart)

---

## Observability Audit

### What Exists
- Free heap (current)
- CPU frequency
- Flash and sketch size
- LittleFS usage
- Uptime (seconds)
- WiFi RSSI, SSID, IP, MAC
- Firmware name, version, build date
- Sensor initialized/status/lastUpdate per sensor
- NTP sync status, last/next sync, drift, server
- MQTT connected/state/lastPublish
- Partition layout (OTA slots, NVS usage)

### What Is Missing

| Missing Metric | Why Needed |
|---------------|-----------|
| `minFreeHeap` | Detect past heap pressure events; required for stability diagnosis |
| `resetReason` | Distinguish normal boot from watchdog, panic, brownout |
| `bootCount` | Track how often device reboots; indicates reliability |
| `wifiReconnectCount` | Track connection instability |
| `mqttReconnectCount` | Track MQTT stability |
| `lastSensorError` per sensor | Last error message per sensor for field debugging |
| `sensorReadCount` / `sensorErrorCount` | Track sensor reliability over uptime |
| `dataTimestamp` in WS payload | Clients can detect stale data |
| `ntpLastSyncEpoch` as absolute time (not millis) | Meaningful for external consumers |
| `taskHighWaterMark` | FreeRTOS stack usage for each task |

### Recommended Diagnostics Payload (additions to `/api/status`)

```json
{
  "diagnostics": {
    "resetReason": 1,
    "bootCount": 42,
    "minFreeHeap": 98304,
    "maxAllocHeap": 65536,
    "wifiReconnectCount": 3,
    "mqttReconnectCount": 1,
    "sensorErrors": {
      "tsl2591": 0,
      "bme280": 2,
      "mlx90614": 0,
      "gps": 0,
      "rg15": 0
    }
  }
}
```

---

## Recommended Remediation Roadmap

### Phase 0: Urgent Fixes (1–3 days)

These must be fixed before the device is used in any observatory automation or safety role.

**Tasks:**
1. Fix `Settings.tsx` to use `POST` instead of `PUT` for config save *(S, UI)*
2. Fix MQTT client ID — use MAC-based identifier *(S, firmware)*
3. Remove passwords from `Config::save()` log output *(S, firmware)*
4. Implement `TCPServer::handleRainRate()` using RG-15 sensor data *(S, firmware)*
5. Set ArduinoOTA password from config *(S, firmware)*

**Rationale:** Items 1, 2, 4 are confirmed bugs that produce wrong behavior. Item 3 is a security issue. Item 5 is a security risk for shared networks.

**Dependencies:** None — all are self-contained changes.

**Expected Outcome:** Config saves work, MQTT connects reliably, ASCOM rain rate is correct, credentials not leaked to serial.

---

### Phase 1: Stabilise Foundations (1–2 weeks)

**Tasks:**
1. Fix async blocking — move WiFi connect, WiFi scan, restart, and RG15 startup delays out of async callbacks *(M, firmware)*
2. Add sensor data mutex — protect sensor struct reads from the async WebServer task *(M, firmware)*
3. Change `serializeJson` for NVS config (remove pretty-print) *(S, firmware)*
4. Add `dataTimestamp` to WebSocket sensor payload and sync broadcast interval to sensor poll interval *(S, firmware)*
5. Mask passwords in `GET /api/config` response *(M, firmware)*
6. Add `minFreeHeap`, `resetReason`, `bootCount` to `/api/status` *(S, firmware)*
7. Fix MQTT timestamp to use epoch time from TimeManager *(S, firmware)*
8. Add MQTT LWT/availability topic *(S, firmware)*
9. Fix screenshots Playwright tests — remove `waitForTimeout`, remove `continue-on-error` *(M, CI/CD/UI)*
10. Pin ESPAsyncWebServer to a specific commit hash *(S, build)*

**Rationale:** Items 1–2 address confirmed crash/reliability risks. Items 3–6 are quick wins that significantly improve operability. Items 7–8 are needed before Home Assistant integration is reliable.

**Dependencies:** Item 2 should precede item 1 (locking must be in place before concurrency changes). Item 8 requires topic schema decision.

---

### Phase 2: Architecture and Testing (2–4 weeks)

**Tasks:**
1. Unify API/WebSocket/MQTT field naming to a single canonical schema *(L, firmware)*
2. Add firmware unit tests with PlatformIO test framework — SkyQuality, CloudDetection, RG15 parser, Config round-trip *(L, firmware)*
3. Add Playwright content-assertion tests for API contract *(M, UI/CI)*
4. Add ASCOM TCP server documentation *(M, docs)*
5. Add config schema reference documentation *(M, docs)*
6. Update REST API docs to match actual response shapes *(S, docs)*
7. Update WebSocket docs to include `rainSensor` *(S, docs)*
8. Update MQTT docs with full payload example *(S, docs)*
9. Fix HDOP unit inconsistency between endpoints *(S, firmware)*
10. Move GPS-disabled sensor creation to not instantiate GPS sensor at all (remove "disabled GPS sensor for API consistency" pattern — replace with `nullptr` + null checks) *(M, firmware)*

**Rationale:** Unified schema prevents integration failures. Tests provide regression coverage for the sensor math. Docs must match code for external integrators.

---

### Phase 3: Polish, Docs, Release Maturity (ongoing)

**Tasks:**
1. Add HTTP Basic Auth option to web server *(M, firmware)*
2. Add N.I.N.A./ASCOM setup guide *(M, docs)*
3. Add Home Assistant integration guide (expanded from current MQTT docs) *(M, docs)*
4. Add Node-RED flow examples *(M, docs)*
5. Add PWA manifest for mobile home-screen install *(S, UI)*
6. Add exponential backoff to `useWebSocket` reconnect *(S, UI)*
7. Add `logLevel` to config, default to INFO *(S, firmware)*
8. Add firmware binary size gate to CI (fail if > 90% of partition) *(S, CI/CD)*
9. Add Dependabot for npm and GitHub Actions deps *(S, CI/CD)*
10. Add CHANGELOG entry for GPS implementation *(S, docs)*
11. Add accessibility improvements — ARIA labels, minimum touch targets *(M, UI)*
12. Fix TCPServer WiFi reconnect — restart server when WiFi reconnects *(S, firmware)*

---

## Suggested GitHub Issues

### Firmware

- **[Bug] Settings save silently fails — UI uses PUT but firmware only accepts POST for /api/config** — Settings page sends `PUT /api/config`. ESPAsyncWebServer only has a POST handler registered. The SPA fallback serves `index.html` with 200, so the save appears to succeed but config is not updated.

- **[Bug] MQTT client ID is set to topic name, causing connection conflicts** — `MQTTClient::connect()` passes `config.topic.c_str()` as client ID. Should be a unique device identifier based on MAC address.

- **[Bug] ASCOM TCP rain rate command (:051) always returns 0.0** — TCPServer has a TODO stub for rain rate. RG-15 sensor is available but not wired to TCPServer.

- **[Critical] Blocking delay() calls inside ESPAsyncWebServer async callbacks** — WiFi connect, WiFi scan, OTA restart, and RG15 reconfiguration all call delay() inside async request handlers. This starves the network stack and can trigger the watchdog.

- **[Bug] Race condition: sensor readings read from async WebServer task while written in loop() task** — No mutex protects sensor struct access between the Arduino loop task and the ESPAsyncWebServer async task.

- **[Security] WiFi and MQTT passwords logged to serial at INFO level** — Config::save() logs the full config JSON including credentials. Should log byte count only.

- **[Improvement] Config stored as pretty-printed JSON in NVS — risks exceeding 4KB limit** — serializeJsonPretty should be replaced with serializeJson. As more sensor configs are added, the NVS key size limit will be hit.

- **[Improvement] Add minFreeHeap, resetReason, bootCount to /api/status** — Critical for field debugging of crashes and reboots.

- **[Bug] LittleFS.begin(true) silently formats filesystem on mount failure** — Should fail explicitly rather than silently erasing all web files.

### API/Telemetry

- **[Bug] MQTT timestamp uses millis() instead of Unix epoch** — Home Assistant and time-series databases expect epoch timestamps. millis() resets on every reboot.

- **[Feature] Add MQTT LWT/availability topic** — Without a Last Will and Testament, subscribers cannot detect when the device goes offline.

- **[Improvement] Unify field names across REST, WebSocket, and MQTT** — `lightSensor` vs `light`, `skyQuality` vs `sky`, etc. causes fragile integrations.

- **[Security] GET /api/config returns WiFi and MQTT passwords in plaintext** — Should mask credential fields in the API response.

### UI

- **[Bug] Rain sensor unit label hardcoded as mm/hr regardless of configured units** — Dashboard shows `mm/hr` even when imperial mode is configured.

- **[Bug] Validation error count divides by 2 — fragile assumption** — `Object.keys(validationErrors).length / 2` is an undocumented hack that will break if path deduplication logic changes.

- **[Improvement] Add data-age indicator to Dashboard** — WebSocket broadcasts stale data 4 of every 5 seconds at default settings. UI should show when the last reading was fresh.

### Testing

- **[Task] Add firmware unit tests for SkyQuality, CloudDetection, RG15 parser** — Zero firmware unit tests currently exist. SkyQuality and CloudDetection are pure functions with no hardware dependencies — ideal for unit testing.

- **[Task] Add API contract tests** — Playwright or Jest tests that verify the mock handler response shapes match TypeScript types.

- **[Bug] Playwright screenshot tests use waitForTimeout — replace with proper wait conditions** — `await page.waitForTimeout(2000)` is explicitly discouraged by Playwright and causes flaky tests.

### Docs

- **[Bug] REST API docs show wrong /api/status response shape** — Docs show flat `{uptime, freeHeap, rssi, ip, version}` but actual response has nested objects.

- **[Bug] /api/wifi/scan docs show wrong response shape and field name** — Docs show array with `secure` field; actual response is `{networks: [...]}` with `encryption` field.

- **[Task] Document ASCOM TCP server (port 2020) commands** — No documentation exists for the TCP/ASCOM protocol implementation.

- **[Bug] CHANGELOG lists GPS as unreleased but it is implemented** — Move GPS from `[Unreleased]` to `[0.0.1]`.

- **[Task] Add config schema reference** — Complete table of all config keys, types, defaults, and valid ranges.

### CI/CD

- **[Bug] Screenshot Playwright CI step silently ignores failures** — `continue-on-error: true` hides regressions in the demo app.

- **[Improvement] Pin ESPAsyncWebServer to specific commit hash** — Currently pinned to GitHub HEAD, which is not reproducible.

- **[Improvement] Key npm cache on package-lock.json, not package.json** — Transitive dep changes aren't caught by current cache key.

### Security

- **[Security] ArduinoOTA has no password** — Any LAN device can push arbitrary firmware. Add password from config.

- **[Security] Unauthenticated settings endpoints can change WiFi credentials** — Consider optional HTTP Basic Auth for config-mutating endpoints.

### Observability

- **[Feature] Add boot diagnostics endpoint** — resetReason, bootCount, minFreeHeap, wifiReconnectCount for field debugging.

- **[Feature] Add lastSensorError per sensor to /api/status** — Currently no way to see the last error message from a failing sensor without a serial monitor.

---

## Final Recommendation

### What is Solid

SQMeter has a genuinely good foundation. The sensor abstraction (`SensorBase`) is clean and extensible. The Preact UI is lightweight, mobile-friendly, and has better form validation than most hobby ESP32 projects. The CI/CD pipeline — including release binaries, complete flash images, and a live demo on GitHub Pages — is well above average. The RG-15 rain sensor integration is thorough and well-documented. The firmware uses C++17 properly with `std::unique_ptr`, `std::optional`, and namespacing throughout.

### What is Risky

**The device should not be used for observatory automation safety decisions in its current state.** Specifically:

1. The ASCOM TCP rain rate command returns 0.0 always — a roof safety interlock will not close during rain.
2. The settings page save does not work (PUT vs POST mismatch).
3. The async blocking in the WiFi connect and OTA restart handlers can destabilize the network stack during configuration operations.
4. There is a race condition between sensor reading and JSON serialization that can produce garbled responses or crashes.
5. Site WiFi passwords are logged to serial and returned via API.

### What Must Be Fixed Before Adding More Sensors

Before adding another sensor (wind, all-sky camera, dew heater controller), the following must be addressed:

1. **Fix the sensor concurrency model** — add a mutex before adding more sensor reads. More sensors = more race condition surface.
2. **Unify the field naming schema** — adding a fourth naming convention for a fifth sensor will make MQTT/REST/WS completely unmaintainable.
3. **Fix the NVS config size issue** — a wind sensor config block will push the pretty-printed JSON over 600 bytes; with 3-4 more sensors it will exceed the NVS limit.
4. **Add at least basic unit tests for sensor math** — adding sensors without tests means regressions in existing calculations (SQM, cloud detection) cannot be caught.

### What Must Be Fixed Before Observatory Automation/Safety Reliance

1. All Phase 0 issues (3 days of work)
2. ASCOM rain rate implementation (already Phase 0)
3. Sensor data mutex (Phase 1)
4. MQTT LWT/availability (Phase 1)
5. Documented and tested sensor failure states — the system must distinguish "sensor unavailable" from "sensor reads 0" clearly in all telemetry channels

After Phase 0 and Phase 1, the device would be suitable as a monitoring device for a human-supervised observatory. For fully automated observatory control (unattended roof, equipment safety), complete the Phase 2 testing work and conduct field testing over at least one full season before relying on it for equipment protection decisions.

---

*End of audit. This report was generated by automated deep inspection of the codebase at commit `0434605`. All findings cite specific files and line numbers verified by reading the actual source. Issues marked "likely" were inferred from code patterns; all others were confirmed by direct code reading.*
