# Research: Share passwords with a paired device

## D1. Where the setting lives

- **Decision**: a new top-level section `sharing` with one switch, `secrets`.
- **Rationale**: it reads as what it does (`sharing.secrets`), leaves room for another shared item
  without overloading `auth`, and costs ~25 bytes of the 5100-byte config.
- **Alternatives considered**: `auth.shareSecrets` (ties it to the login section, though the login
  also protects many other things); a key under `mqtt` or `alerts` (the four secrets span both).

## D2. Order of the checks

- **Decision**: setting off → 403; no device password → 403; failed-login lock → 429; login →
  401; else 200.
- **Rationale**: both 403 conditions are already public in the unauthenticated `GET /api/config`, so
  answering them first leaks nothing and tells a paired device what the owner must change. The
  lock comes before the login check so a locked client can't keep testing passwords. Without a
  device password `requireAuth` lets every request through, so that condition must be checked
  before it.
- **Alternatives considered**: login first (prompts for a password even when the answer can only
  be a refusal; with no device password the login check passes everything anyway).

## D3. Failed-login protection

- **Decision**: `LoginLock`: five failed logins in a row lock the request for 60 s; a success
  resets the count; one lock for the device, in RAM.
- **Rationale**: SQMeter has no failed-login protection; a small lock on the one request that
  returns secrets bounds guessing there (SC-005) at no cost to the paired device, which sends the
  right login. Per-client tables need memory and are easy to dodge on a LAN.
- **Alternatives considered**: protecting every `requireAuth` route (changes the behaviour of every
  write route and the web UI's own prompts; out of scope); a per-client lock (more RAM, IPs are
  trivially changed on a LAN).
- **Trade-off**: a LAN client that keeps failing can make the paired device wait a minute. The
  paired device copies settings on request, so a short wait is acceptable.

## D4. Shape of the answer

- **Decision**: the contract's nested shape, every key always present, `""` when unset; built with
  ArduinoJson into a `std::string`.
- **Rationale**: the paired device is built against this exact shape in parallel; matching the
  `GET /api/config` nesting (`mqtt.password`, `alerts.ntfy.token`, `alerts.pushover.*`) lets it
  merge the values into the settings it already copies.

## D5. Caching and logging

- **Decision**: `Cache-Control: no-store` on every answer the handler builds (200, 403, 429); the
  401 is the server's standard challenge. One info log line per share (client address only), one
  warning when the lock starts.
- **Rationale**: FR-008/FR-009; the device log is shown in the web UI and over serial.

## D6. The demo

- **Decision**: `EmulatedDevice::secretsRequest()` answers with the same pure functions; no login
  (the demo has none).
- **Rationale**: the API explorer and OpenAPI examples come from the demo (spec 028); a missing
  route would answer 404 there.

## D7. Plain HTTP

- **Decision**: document that the values cross the LAN unencrypted; SQMeter has no TLS
  (Principle VI) and this feature adds none.
