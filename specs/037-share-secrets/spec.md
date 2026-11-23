# Feature Specification: Share passwords with a paired device

**Feature Branch**: `spec/037-share-secrets`

**Created**: 2026-11-23

**Status**: Draft

## Context

RoofRoof, the roll-off roof controller that runs at the same observatory, copies SQMeter's site
settings so they don't have to be typed twice: the MQTT broker, port, user and topic, the time
servers and time zone, the location, and the ntfy and Pushover channels. SQMeter masks every
secret in its settings answer, so the MQTT password, the ntfy token and the Pushover keys still
have to be typed by hand on RoofRoof.

**Share passwords** is an owner-chosen setting, off by default. While it is on and the device
password is set, a device that knows the device password can ask SQMeter for exactly those four
stored values. Nothing else is ever shared: not the WiFi password, the device password, the
upload password, the Bluetooth passkey or the webhook header.

## Clarifications

### Session 2026-11-23

Answered with the safest option the constitution allows; the owner reviews them on the pull request:

- Q: Which check answers first when several fail? → A: setting off, then no device password, then
  the failed-login lock, then the login (see Assumptions).
- Q: May the setting be saved on while no device password is set? → A: Yes, like the upload
  password (D-32): it is saved and reported not in effect (D-53); requests are refused.
- Q: What protects the login from guessing? → A: a lock on this request after five failed logins
  in a row, for 60 seconds (FR-007).
- Q: Is a secret shared when its channel is off? → A: Yes, as stored (edge cases).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - RoofRoof copies the passwords too (Priority: P1)

The owner has password-protected SQMeter's changes and turns on **Share passwords**. On RoofRoof
they copy the site settings from SQMeter, giving SQMeter's username and password. RoofRoof now
has the MQTT password, the ntfy token and both Pushover keys, and the owner types nothing more.

**Why this priority**: this is the whole point of the feature.

**Independent Test**: with sharing on and a device password set, a request with the right login
returns the four stored values; unset ones come back empty.

**Acceptance Scenarios**:

1. **Given** sharing is on, a device password is set and the MQTT password, ntfy token and both
   Pushover keys are stored, **When** a client asks for the shared passwords with the right login,
   **Then** it receives those four values exactly as stored.
2. **Given** the same but no ntfy token is stored, **When** a client asks, **Then** the ntfy token
   is an empty string and the other values are unchanged.
3. **Given** a successful answer, **Then** the answer tells clients and proxies not to store it,
   and the device's log records that passwords were shared, with no value in it.

---

### User Story 2 - Nothing leaks unless the owner allows it (Priority: P1)

Sharing is off unless the owner turns it on, and even then it needs the device password. Asking
without permission gets a clear refusal and no secret.

**Why this priority**: the device is on a LAN that other devices share; a wrong default would
expose the owner's push and broker credentials.

**Independent Test**: every combination of setting off, no device password, missing or wrong
login gets a refusal without any secret in it.

**Acceptance Scenarios**:

1. **Given** sharing is off, **When** a client asks, **Then** it is refused with "Password sharing
   is off in SQMeter".
2. **Given** sharing is on but password protection is off or has no password, **When** a client
   asks, **Then** it is refused with "Set a web password in SQMeter before sharing passwords".
3. **Given** sharing is on and a device password is set, **When** a client asks with no login or a
   wrong one, **Then** it gets the device's usual "password needed" answer.
4. **Given** a client has sent a wrong login five times in a row, **When** it asks again within a
   minute, **Then** it is told to wait, without the login being checked, and the lock lifts after
   that minute.
5. **Given** any number of requests without a login, **When** the paired device then asks with
   the right login, **Then** it gets the values: requests that guess nothing never lock it out.

---

### User Story 3 - The owner understands what they are turning on (Priority: P2)

In **Settings → Device → Security**, below the device password, the owner finds **Share
passwords**. Its "?" says what is shared and who can read it. While it is on, a short note warns
that anyone with the device password can read these over the network unencrypted. While password
protection is off, the switch is disabled with the reason and a link to set the password.

**Why this priority**: the setting is a security trade-off; the owner must see it before turning
it on.

**Independent Test**: open the Security card; the switch is off by default, disabled without a
device password, enabled once one is set, and shows the warning when on.

**Acceptance Scenarios**:

1. **Given** password protection is off, **When** the owner opens Security, **Then** Share
   passwords is disabled and says to set a device password, with a link to it.
2. **Given** password protection is on with a password, **When** the owner turns Share passwords
   on and saves, **Then** the setting is stored, survives a restart, and the warning note shows.

### Edge Cases

- The owner turns sharing on, then turns password protection off: sharing stays saved but is not
  in effect (shown as such in Settings and in the settings-in-effect report), and requests are
  refused with the "set a web password" message.
- A secret is stored but its channel is off (for example ntfy disabled with a token kept): the
  stored token is still shared, since the client copies what is stored.
- A settings backup or restore carries the Share passwords setting like any other setting.
- A restart forgets the failed-login count.
- The demo has no device password; it answers with the demo's own settings so the API explorer
  shows each answer.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST have a setting `sharing.secrets` (on/off), off by default and off for
  settings saved by an older release.
- **FR-002**: The device MUST answer a shared-passwords request with exactly four values: the MQTT
  password, the ntfy token, the Pushover user key and the Pushover application token, each as
  stored or empty when not set, in the shape given in [contracts/api.md](contracts/api.md).
- **FR-003**: The answer MUST NOT contain any other secret or setting.
- **FR-004**: With the setting off, the device MUST refuse with "Password sharing is off in
  SQMeter" (forbidden).
- **FR-005**: With no device password in effect (password protection off, or no password), the
  device MUST refuse with "Set a web password in SQMeter before sharing passwords" (forbidden).
- **FR-006**: The request MUST pass the device's existing login check; a missing or wrong login
  gets the existing "password needed" answer.
- **FR-007**: After five wrong logins in a row on this request, the device MUST refuse further
  attempts for 60 seconds without checking the login (too many requests, with the seconds to
  wait). Only a request that sends Basic credentials which don't match counts; a request without
  credentials never does. A successful login, or 10 minutes without a wrong login, resets the
  count.
- **FR-008**: Every answer the device builds for this request (200, 403, 429) MUST tell clients
  not to store it; the 401 is the server's standard challenge and carries nothing secret.
- **FR-009**: The device MUST NOT log any shared value; it MAY log that passwords were shared and
  to which address.
- **FR-010**: The setting MUST appear in Settings → Device → Security with a "?" explanation of
  what is shared, a warning note while it is on, and a settings dependency (D-53) that disables it
  with a reason and a fix link while no device password is set.
- **FR-011**: The settings answer (`GET /api/config`) MUST keep masking every secret; it shows
  `sharing.secrets` like any other setting.
- **FR-012**: The REST reference, the OpenAPI description, the security and configuration pages
  of the user guide and the settings-dependencies reference MUST describe the request, what it
  shares, and that it travels as plain HTTP.

### Key Entities

- **Share passwords setting** (`sharing.secrets`): whether the request may answer at all.
- **Shared passwords**: the four stored values, in the contract's shape.
- **Failed-login lock**: the count of failed logins in a row on this request and, after five,
  when the lock lifts.

## Design *(mandatory for UI changes)*

- **DS rules**: DS-04 (the explanation behind "?", the warning as a one-line note), DS-20 (label
  "Share passwords"), DS-22 (note within 90 characters), DS-23 (hint within 160 characters),
  DS-25, DS-27 (one name: "device password" as in the backup card), and the dependency rule
  for settings that need another setting (Principle V): disabled with the reason and a fix link.
  It is one toggle in the existing Security card, from the shared controls.
- **New strings (English)**: label "Share passwords"; hint "Lets a paired device, such as RoofRoof,
  copy the MQTT password, ntfy token and Pushover keys using the device password."; note, shown while it is on
  and in effect, "Anyone with the device password can read them over plain HTTP."; dependency reason "Set a device
  password" (short: "Set password").
- **Mockup**: see Assumptions; the owner reviews the Security card change on the pull request
  before it merges. Images are kept outside the repository.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With sharing on and a device password set, a paired device obtains all four stored
  values in one request, and the owner types none of them on the paired device.
- **SC-002**: In automated tests, 100% of requests made with sharing off, without a device
  password, or with a missing or wrong login return no secret.
- **SC-003**: No answer of the device, other than this one request, contains any of the four
  values unmasked (the existing masking tests still pass).
- **SC-004**: An owner can find the setting, understand what it shares and turn it on in under a
  minute, without reading the docs.
- **SC-005**: A client guessing the login can make at most five attempts per minute on this
  request.

## Assumptions

### Assumptions decided for the owner

- **Name and place**: the setting is `sharing.secrets`, a new `sharing` section, shown as **Share
  passwords** in Settings → Device → Security, below the password fields.
- **Order of checks**: setting off (403), then no device password (403), then the failed-login lock
  (429), then the login (401), then the answer (200). Both 403 answers reveal only what the
  unauthenticated `GET /api/config` already shows (whether sharing and password protection are on),
  and checking them first means a client with sharing off gets the reason, not a password prompt.
- **"Device password set"** means password protection is on and its password is not empty: the
  same condition under which the device asks for a login at all.
- **Rate limit**: SQMeter had no failed-login protection. This request gets its own: five failed
  logins in a row lock it for 60 seconds, device-wide (not per client), forgotten on restart or
  after 10 minutes without a wrong login. Only requests that send Basic credentials count; a
  request without a login (a browser's first try, a stray request from any page on the LAN) does
  not, so it can't be used to lock the paired device out. The lock applies to this request only
  and doesn't protect the password: the device's other password-protected requests are unchanged,
  and the security page says so.
- **No https**: SQMeter has no TLS, so the values cross the LAN as plain HTTP. The docs and the
  warning note say so.
- **Values of channels that are off** are shared as stored, because the client copies settings,
  not what is in use.
- **Logging**: one info line per successful share naming the client address, one warning when the
  lock starts; never a value.
- **Constitution**: Principle VI says secrets MUST be masked in API responses. This feature is a
  deliberate, narrow exception, so the PR amends Principle VI (MINOR, 1.4.0 → 1.5.0): one opt-in,
  password-protected request may return the MQTT password, ntfy token and Pushover keys; every
  other answer keeps masking.
- **Mockup review**: spec 026 asks for an owner-reviewed mockup before building UI. This change was
  built without waiting for that review; the UI is a single toggle, its note and its dependency
  reason in an existing card, from the shared controls. The owner reviews it on the pull request,
  where it can be changed before merging.
- **Translations** are made as part of the change for every language and recorded with the
  project's translation tool (`tools/i18n/record.json`), like spec 036. No paid AI service is wired
  into CI.
- **Demo**: the demo answers the request from its own settings and, like every demo request, does
  not ask for a login.
