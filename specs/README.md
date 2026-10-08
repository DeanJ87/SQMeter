# Specifications

Feature specs for [Spec Kit](https://github.com/github/spec-kit). The rules they are checked
against are in [`.specify/memory/constitution.md`](../.specify/memory/constitution.md).

## Backfill (v0.2.0)

Specs 001–014 describe the system as it existed at v0.2.0 (`main` @ `b1d382e`). Each was written
from the documentation, the maintainer's stated requirements and the code, then checked with
`/speckit-converge`. The gaps it found are the **Convergence** phase at the end of each
`tasks.md`: open, traceable tasks (`/speckit-implement` works through them).

| Spec | Feature | Open convergence tasks |
|---|---|---|
| [001](001-sky-quality/spec.md) | Sky quality (SQM, NELM, Bortle, calibration) | 0 — converged |
| [002](002-environment-cloud/spec.md) | Environment and cloud cover | 0 — converged |
| [003](003-rain-sensor/spec.md) | Rain sensor (RG-15) | 0 — converged |
| [004](004-wind/spec.md) | Wind | 0 — converged |
| [005](005-time-location/spec.md) | Time, location and Sun & Moon | 0 — converged |
| [006](006-safety-monitor/spec.md) | Safety monitor | 0 — converged |
| [007](007-ascom-alpaca/spec.md) | ASCOM Alpaca | 0 — converged |
| [008](008-alerts/spec.md) | Alerts | 0 — converged |
| [009](009-bluetooth/spec.md) | Bluetooth | 0 — converged |
| [010](010-dashboard/spec.md) | Dashboard, demo and screenshots | 0 — converged |
| [011](011-settings-security/spec.md) | Settings, configuration and security | 0 — converged |
| [012](012-ota-updates/spec.md) | Firmware and web UI updates | 0 — converged |
| [013](013-data-interfaces/spec.md) | MQTT, REST and WebSocket | 0 — converged |
| [014](014-wifi-setup/spec.md) | WiFi setup and network presence | 0 — converged |

All convergence tasks were implemented in the v0.2.0-beta.2 work (PR #75); the tasks stay in
each `tasks.md`, checked off, as the record.

New features start at 015 with `/speckit-specify`.
