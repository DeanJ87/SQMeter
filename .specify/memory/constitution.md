# SQMeter Constitution

## Core Principles

### I. Fail-Safe Safety Verdict

The SafetyMonitor verdict protects equipment, so doubt MUST resolve to "unsafe".

- Missing, stale or faulted data MUST make the verdict unsafe; a zeroed or stale reading MUST
  NOT be compared against a threshold as if it were real.
- Rain MUST be evaluated independently of every other sensor's freshness, so a stale or faulted
  sky sensor can never hide rain.
- A limit that can't be measured (e.g. a wind limit with no working anemometer) MUST count as
  unsafe.
- Every unsafe reason MUST state the measured value and the configured limit
  (`SQM 18.21 < 19.50`), not just that a rule failed.
- Alerts MUST never be able to change the verdict served to Alpaca clients.

### II. Standards Conformance (ASCOM Alpaca)

N.I.N.A. and other clients depend on the device behaving to the Alpaca specification.

- The Alpaca HTTP API lives in `lib/AlpacaLogic` (`Alpaca::Router`); the firmware and
  `tools/alpaca-sim` MUST both use it, never a second implementation.
- Every change to it MUST pass ASCOM ConformU (conformance and the Alpaca protocol check, for
  both devices) with zero errors, issues and configuration alerts in CI.
- Advertised interface versions MUST match what is implemented; anything unsupported MUST
  return NotImplemented rather than an invented value.
- Device UniqueIDs and discovery MUST stay stable across releases; a change that forces clients
  to re-select devices MUST be called out in the release notes.

### III. Testable Pure Logic

Decision logic MUST be testable on a desktop without hardware.

- Safety, alert, Alpaca protocol, Bluetooth, wind and astronomy logic MUST live in `lib/*`
  without Arduino dependencies and be covered by native Unity tests (`pio test -e native`).
- Web UI logic and components MUST be covered by Vitest tests; pages and their behaviour are
  exercised with the MSW mocks.
- A bug fix MUST add a test that fails without the fix, unless the fix is in untestable glue,
  in which case the PR states how it was verified instead.

### IV. Embedded Resource Budgets

The ESP32 has fixed memory and flash; exceeding them breaks devices in the field.

- Firmware MUST build with `-Wall -Wextra -Werror` and zero warnings, for both the standard and
  the Bluetooth build.
- Both firmware images MUST fit their OTA app slots (standard 1.5 MB, Bluetooth 1.69 MB); a PR
  that adds more than ~20 KB of flash states the new usage.
- Request handlers on the async TCP task MUST NOT block on the network or on long sensor work;
  HTTPS sends run on their own task, one TLS session at a time.
- Changes that add heap or stack use MUST be measured on a device (`/api/status` heap and
  `stackFree`) and keep the main loop stack headroom above 2 KB.
- Persisted config MUST stay within its NVS limits (main JSON 5100 bytes, alerts JSON 3900
  bytes) and be validated before saving.

### V. Quiet, Consistent UI

The web UI is a tool used in the dark, often on a phone.

- Components MUST reuse the shared building blocks (`web/src/components/ui.tsx`,
  `web/src/components/settings/controls.tsx`) and existing CSS classes; no new button or
  card styles.
- Explanations go in "?" tooltips or short notes, not paragraphs; copy states facts plainly
  and doesn't over-explain.
- Settings that depend on missing or disabled hardware MUST be disabled with the reason and a
  link to fix it; cards for disabled sensors MUST NOT appear on the dashboard.
- Every page MUST work at phone width.

### VI. Trusted-LAN Security

The device is designed for a trusted LAN and has no TLS of its own.

- Every endpoint that changes state, sends something or reveals more than readings MUST call
  `requireAuth`.
- Secrets (passwords, keys, tokens, passkeys) MUST be masked in API responses and preserved
  when the masked value is sent back.
- Nothing may imply the device is safe to expose to the internet.
- The maintainer's private email address MUST NOT appear in any public file.

### VII. Docs Move With Behaviour

- A user-visible change MUST update `docs/` in the same PR: the user guide, the REST and
  WebSocket references and the configuration reference, as applicable.
- `mkdocs build --strict` MUST pass.

## Platform Constraints

- **Firmware:** C++17 on Arduino-ESP32 2.0.17 via PlatformIO; ESPAsyncWebServer (handlers
  match by prefix, so register specific routes before general ones); ArduinoJson 6; NimBLE
  for the Bluetooth build. Code follows `CONTRIBUTING.md`: enums and structs over strings,
  RAII, const-correctness, `std::string` in logic code.
- **Web UI:** Preact + Vite, TypeScript strict (no `any`), Zod validation of config, served
  from LittleFS. Firmware and web UI ship as a matched pair in every release.
- **Hardware:** I2C on 21/22, GPS on 16/17 and the RG-15 on 18/19 are reserved; analog inputs
  MUST use ADC1, which keeps working while WiFi is on.
- **Compatibility:** config saved by an older release MUST load and be migrated without a
  factory reset. Breaking changes (partition layout, UniqueIDs, removed settings) MUST be listed
  in the release notes with what users need to do.
- **Updates:** OTA MUST be atomic; a failed or interrupted update leaves the previous firmware
  running.

## Development Workflow and Quality Gates

- Work happens on a branch from `main` and lands through one pull request per feature or fix,
  targeting `main` directly. Don't stack long chains of dependent PRs.
- Nothing is committed directly to `main`. Commits and PRs carry no AI co-author or attribution
  lines.
- A PR merges only when CI passes:
  - the `build` workflow: typecheck, web tests with coverage, native tests, both firmware
    builds, the LittleFS image and the integrity checks;
  - ConformU against the Alpaca simulator;
  - the docs build.
- Firmware behaviour changes SHOULD be verified on a real device before release (OTA to the
  test device, with the owner's permission when it's in use), and the PR says what was checked.
- Releases are cut by pushing a `v*` tag; a tag with a pre-release suffix (`-beta.N`) is
  published as a GitHub pre-release for the beta update track.
- New features follow Spec Kit: `/speckit-specify` → `/speckit-plan` → `/speckit-tasks` →
  `/speckit-implement`, repeating `/speckit-converge` until it reports converged. Specs live in
  `specs/<NNN-feature>/`. A feature's `spec.md` is a living contract while its PR is open, and
  a historical record once merged; later changes get a new spec.

## Governance

- This constitution takes precedence over other practice in this repository. `CONTRIBUTING.md`
  and the docs MUST NOT contradict it; where they do, they are corrected.
- Amendments are made by pull request editing this file, with the version bumped by semantic
  versioning:
  - MAJOR: a principle is removed or redefined incompatibly;
  - MINOR: a principle or section is added or materially expanded;
  - PATCH: wording and clarifications.
- `/speckit-plan`, `/speckit-analyze` and `/speckit-converge` check work against these
  principles. A plan that must break one records the violation and why in its Complexity
  Tracking section.
- Each PR description notes which principles the change touches and how they were satisfied.

**Version**: 1.0.0 | **Ratified**: 2026-10-08 | **Last Amended**: 2026-10-08
