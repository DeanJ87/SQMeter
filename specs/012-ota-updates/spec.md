# Feature Specification: Firmware and Web UI Updates

**Feature Branch**: n/a — backfilled from `main` @ `b1d382e` (v0.2.0)

**Created**: 2026-10-08

**Status**: As-built (backfill)

**Input**: User description: "Backfill updates: self-update from GitHub releases (stable/beta tracks), manual upload, command-line uploads, and the release pipeline that feeds them."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Update from the browser in one click (Priority: P1)

The owner opens Updates, picks the stable or beta track, sees whether a newer release exists and
installs it; firmware and web UI update together.

**Acceptance Scenarios**:

1. **Given** a track, **When** checked, **Then** releases on that track with both required assets
   are listed, and "newer" is decided by comparing semantic versions with the running firmware.
2. **Given** an install, **When** it runs, **Then** firmware and web UI are flashed as a matched
   pair over pinned-certificate HTTPS; any failure leaves the previous firmware running (a failure
   after the web UI is written can leave the new web UI until retried - see FR-002).
3. **Given** the Bluetooth build, **When** updating, **Then** the Bluetooth firmware asset is used.

---

### User Story 2 - Install a file manually (Priority: P2)

The owner uploads a firmware or web UI image on the Updates page.

**Acceptance Scenarios**:

1. **Given** a valid image, **When** uploaded, **Then** it's flashed and the device restarts —
   reliably on the first attempt.
2. **Given** a failure, **When** it happens, **Then** an error status code and message are returned.

---

### User Story 3 - Releases that describe themselves (Priority: P2)

Every release reports its own version, and a local build is distinguishable from a release.

**Acceptance Scenarios**:

1. **Given** a tagged release, **When** installed, **Then** it reports the tag's version.
2. **Given** a local/dev build, **When** running, **Then** its version is at or above the last
   release with a dev marker, so the update check doesn't offer older releases as newer.

### Edge Cases

- GitHub unreachable: clear error, nothing changes.
- Low heap during TLS: the check still works (document allocated before TLS).
- Partition layout change (standard ↔ Bluetooth): requires USB.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device MUST list releases from GitHub for the stable or beta track, matching
  firmware and web UI assets for its build variant.
- **FR-002**: Installing MUST flash both images over HTTPS with pinned roots. The boot slot MUST only
  change after the firmware is fully written and verified, so any failure leaves the previous
  firmware booting. *(Corrected: not atomic. The web UI is written first, so a failure after it can
  leave a newer web UI with the old firmware until the update is retried; the docs say so. The
  bootloader only rolls back a firmware that is invalid or never starts - one that starts and later
  crashes is kept (found by spec 024).)*
- **FR-003**: Manual upload MUST accept firmware or web UI images and succeed on the first attempt
  for a valid image over a normal link. If switching the boot slot fails once after a complete,
  valid write, the device MUST verify and retry the switch itself, and report `retried` so the
  first-attempt rate can be measured.
- **FR-004**: Upload and update failures MUST return a non-2xx status with an error message.
- **FR-005**: Each build MUST report a version that orders correctly against releases (tags inject
  their version; local builds carry a dev version ≥ the last release).
- **FR-006**: Release-selection logic (track filter, asset matching, version comparison) MUST be
  tested off-device.
- **FR-007**: The OTA documentation MUST use the Updates page's current labels.

### Key Entities

- **Release**: tag, prerelease flag, firmware and filesystem asset URLs and sizes.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: ≥ 99% of manual uploads of valid images succeed on the first attempt on a normal link.
- **SC-002**: The update check never offers a release older than the running build as newer.

## Assumptions

- Command-line (ArduinoOTA) uploads are an advanced, opt-in path.
