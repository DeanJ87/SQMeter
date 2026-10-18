# Research: Settings backup and restore

Decisions for [plan.md](plan.md), grounded in the code on `main` (v0.3.4-beta.1+dev).

## R1. What the settings look like today

- `Config::toJson(redactSecrets, includeAlerts)` (`lib/ConfigModel/src/ConfigJsonOut.cpp`) writes every
  section; with `redactSecrets` each non-empty secret becomes `SECRET_MASK` (`"********"`) and an empty one
  stays `""`. `GET /api/config` is `toJson(true)`, alerts included (`alertsToJson(…, AlertsPart::All)` shape
  nested under `"alerts"`).
- `Config::applyJson(json, cfg, preserveSecretPlaceholders=true)` (`ConfigJsonIn.cpp`) is what a UI/API save
  uses through `Config::fromJson(json, &current)`: a missing key keeps the current value, a wrong type is
  refused naming the field, `assignSecret` keeps the current secret for the mask or `""` and clears it for
  `null`, then `validate()` and `validateIncoming()` (address forms, Pushover key format) run. A blank WiFi
  network is refused; a different non-blank one is accepted.
- There are nine secrets: `wifi.password`, `mqtt.password`, `ota.password`, `auth.password`, `ble.passkey`,
  `alerts.pushover.userKey`, `alerts.pushover.appToken`, `alerts.ntfy.token`, `alerts.webhook.authHeader`.
- `WebServer::handleSetConfig` (`src/WebServerApi.cpp`) calls `requireAuth`, `Config::fromJson`, then
  `saveConfigCallback` (which bumps `configRevision`, so open pages reload), then re-applies the sky
  averaging and the language. Restart-only settings are decided in the web UI only
  (`web/src/components/settings/restart.ts`); the firmware never restarts itself after a save.
- Size: the main JSON is capped at 5100 bytes, each alerts part at 3900 (`Config::save`). A full
  `toJson(true)` is at most about 13 KB with every alert wording at its limit, typically 4-6 KB.

**Decision**: the backup's `settings` object is exactly `toJson(true)` (FR-002: "same shape as the settings
API"), so a backup made without a passphrase is the old curl export plus a header, and restoring goes
through `applyJson` with placeholders preserved: the same validation and messages as any save (FR-009).

## R2. Crypto

- **Decision**: PBKDF2-HMAC-SHA256 (16-byte random salt, **20,000 iterations**, 32-byte key) and
  AES-256-GCM (12-byte random nonce, 16-byte tag, associated data `sqmeter-backup/1`). The passphrase and the
  key are never stored or logged.
- **Firmware**: mbedTLS from ESP-IDF 5.5, already linked for TLS: `mbedtls_pkcs5_pbkdf2_hmac_ext`
  (`MBEDTLS_PKCS5_C` is on in `esp_config.h`) and `mbedtls_gcm_*` (`MBEDTLS_GCM_C` on; AES-GCM is used by
  the TLS client). Random salt, nonce and token from `esp_fill_random` (hardware RNG; WiFi is on). Only
  the thin adapter (`src/BackupCryptoMbed.cpp`) touches mbedTLS.
- **Native tests and the demo**: `lib/ConfigBackup/src/PortableCrypto.cpp`, a small portable SHA-256 / HMAC /
  PBKDF2 / AES-256 / GCM implementation compiled only where `ESP_PLATFORM` is not defined (native and
  WebAssembly). The native environment has no mbedTLS (`[env:native]` pulls only ArduinoJson, and CI's
  Ubuntu runner has no `libmbedtls-dev`). It is real crypto, held to published vectors in the native
  tests: FIPS 180-4 SHA-256, RFC 4231 HMAC, RFC 7914 §11 PBKDF2-HMAC-SHA256 and the NIST GCM spec
  (AES-256 test cases 13-16), plus a backup encrypted by an independent implementation (Node's WebCrypto)
  and checked into `test/fixtures/backup/`. The firmware adapter and the portable code implement the same
  three functions (`Backup::deriveKey`, `seal`, `open`), so the file format code is shared.
- **Iteration count**: the cost is chosen for the slowest supported board. mbedTLS PBKDF2 does four
  SHA-256 compressions per iteration; on a 240 MHz ESP32 that is about 50-100 µs per iteration (estimated
  from mbedTLS SHA-256 throughput on Xtensa and the ~1.3 s ESP-IDF measured for 2×4096 PBKDF2-SHA1
  iterations before it adopted fast_pbkdf2), so 20,000 iterations is about 1-2 s per derivation. It runs
  once per backup and once per restore preview; apply uses the staged result. The iteration count is in
  the file, so a later release can raise it without breaking old files. Files are accepted with 1,000 to
  100,000 iterations, so a crafted file can't hold the device for minutes. The firmware logs each
  derivation's measured time (`Backup key derived in N ms`) so the cost can be confirmed on the test
  device; the native build of the portable code takes about 15 ms for 20,000 iterations on a 2024 laptop.
- **Alternatives**: scrypt/Argon2 (memory-hard, better against GPUs) - not in ESP-IDF's mbedTLS and need
  tens of KB of RAM per derivation; ChaCha20-Poly1305 - in mbedTLS but not in WebCrypto, which the docs
  can point to for decrypting a file by hand; HKDF - not a slow derivation (FR-004).
- **Threat model** (constitution VI): the file protects the secrets at rest (a lost USB stick, a shared
  backup folder). The device is a trusted-LAN appliance without TLS; the passphrase travels to the device
  over HTTP like the device password does. The docs say so.

## R3. Not blocking the web server

- `handleUpdatesCheck` (`src/WebServerUpdates.cpp`) already answers a paused request (`request->pause()`,
  `AsyncWebServerRequestPtr`) from its own FreeRTOS task, one at a time behind an atomic flag.
- **Decision**: backup and restore preview run the same way: the handler checks auth and size, copies the
  current config, and starts one `backup_job` task (one at a time; 409 while one runs). The task is pinned
  to core 1 at priority 1, beside the Arduino loop task, so time slicing keeps the loop running and the
  AsyncTCP task (higher priority) keeps serving pages during the 1-2 s derivation; core 0's idle task,
  which the task watchdog watches, is never starved. Apply needs no derivation and runs in the handler.
- **Alternative**: chunked derivation on the loop task - needs a hand-written PBKDF2 loop on the firmware
  and slows the sensor loop for longer.

## R4. Preview bound to apply

- **Decision**: the preview computes the complete candidate config (current settings + backup settings +
  decrypted secrets, minus what can't apply) and validates it exactly as a save would. The firmware keeps
  that candidate with a random 128-bit token and the `configRevision` it was computed from, for 10 minutes;
  `POST /api/backup/apply {"token"}` saves that stored candidate, so what is applied is exactly what was
  previewed (FR-013). A new preview replaces it, apply consumes it, and a settings change in between
  (revision differs) refuses the apply with 409 so the owner previews again.
- **Alternative**: the client resends the file and passphrase on apply - a second slow derivation, and
  the device couldn't prove the two matched.

## R5. Restart and cut-off warnings

- **Decision**: the restart rule moves into the firmware as `Backup::restartReasons(before, after, build)`
  with the same codes and rules as `restart.ts` (WiFi, command-line uploads, GPS, I2C, IPv6, Alpaca
  discovery, Bluetooth), held together by a shared fixture (`test/fixtures/backup/restart-cases.json`)
  run by the native tests and Vitest. Apply restarts the device (`scheduleRestart`) only when a reason is
  present (FR-008, US2.2).
- Warnings, as codes the UI words: `wifi-network` (other SSID: may become unreachable; the WiFi setup
  hotspot is the way back), `device-password` (password protection on and its username or password
  changes, or it is switched on), `address` (hostname or local name changes), `imaging-app` (a restart
  while an Alpaca client is connected or watched), `newer-firmware` (the backup's firmware is newer).

## R6. Boards and versions

- Board profiles for the ESP32-S3 boards are separate, unmerged work. **Decision**: the board's valid pins
  come in as a `Backup::BoardPins` value (`validPins` and `analogPins` bit masks); this firmware passes
  the classic ESP32 set (`isValidGpio`, ADC1 32-39). A pin setting whose value isn't valid here is not
  applied, keeps this board's value and is listed with reason `pin` or `analog`. Bluetooth settings on the
  standard build are listed with reason `bluetooth` and not applied. Pushover switched on in the backup,
  with the keys neither restored nor present here, is listed with reason `pushover-keys` (otherwise the
  save would be refused for missing keys, and US3.3 says the rest restores).
- **Versions**: the file has its own format number (`sqmeterBackup: 1`). A larger number is refused
  (`newer-format`); a plain settings object without the header (the old curl export) is accepted as format
  0: settings only, no metadata, secrets kept. Firmware versions are compared numerically
  (`major.minor.patch`, then the pre-release number) only to warn about a backup from newer firmware;
  settings a newer firmware wrote that this one doesn't know are listed as ignored, settings an older one
  lacks are listed as kept.

## R7. Upload handling and limits

- `AsyncCallbackJsonWebHandler` caps bodies at 16384 bytes by default and parses into a separate document.
  **Decision**: backup and preview collect the raw body themselves (one buffer, capped at
  **20480 bytes**, 413 above it before parsing) and hand it to the job; the lib parses it once, strings in
  place where it can. The web UI refuses a file over 64 KB before reading it (pretty-printed files are
  larger than what it sends) and sends the backup compact.
- Heap: a typical preview holds the body, two settings documents and the candidate (about 25-30 KB);
  the worst case (every alert wording at its limit) about 60 KB, transiently, in the job task. To be
  measured on the spare device (`/api/status` `freeHeap`) before release.

## R8. Demo

- The demo's device core is `lib/` compiled to WebAssembly (`tools/demo-core/build.sh`). **Decision**:
  `EmulatedDevice` gets `backup`, `restorePreview` and `restoreApply`, calling the same `lib/ConfigBackup`
  code (the portable crypto), with random bytes passed in from `crypto.getRandomValues`. The MSW handlers
  answer `/api/backup*` from it (FR-014). No crypto is written in TypeScript (STRUCT-07).

## R9. UI

- One card, **Backup & restore**, on Settings → Device after Security, built from `SettingsCard`, `Field`,
  `TextInput`, `ActionButton`, `Group`, `ReadingRow`, `Note` and `InfoTip` (DS-01, DS-03, DS-04, DS-06,
  DS-07, DS-22, DS-23). Mockups first ([mockups/README.md](mockups/README.md)).
- The download is made in the browser from the device's answer (pretty-printed, named
  `sqmeter-<device>-<YYYY-MM-DD>.json`); the device also sends `Content-Disposition` for curl users.
