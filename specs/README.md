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
| [001](001-sky-quality/spec.md) | Sky quality (SQM, NELM, Bortle, calibration) | 7 (1 critical) |
| [002](002-environment-cloud/spec.md) | Environment and cloud cover | 6 (1 critical) |
| [003](003-rain-sensor/spec.md) | Rain sensor (RG-15) | 4 (1 critical) |
| [004](004-wind/spec.md) | Wind | 3 |
| [005](005-time-location/spec.md) | Time, location and Sun & Moon | 3 |
| [006](006-safety-monitor/spec.md) | Safety monitor | 3 (1 critical) |
| [007](007-ascom-alpaca/spec.md) | ASCOM Alpaca | 2 |
| [008](008-alerts/spec.md) | Alerts | 4 |
| [009](009-bluetooth/spec.md) | Bluetooth | 2 |
| [010](010-dashboard/spec.md) | Dashboard, demo and screenshots | 4 |
| [011](011-settings-security/spec.md) | Settings, configuration and security | 2 |
| [012](012-ota-updates/spec.md) | Firmware and web UI updates | 5 |
| [013](013-data-interfaces/spec.md) | MQTT, REST and WebSocket | 13 (1 critical) |
| [014](014-wifi-setup/spec.md) | WiFi setup and network presence | 5 |

Some documentation tasks are already addressed by the docs refresh PR (#73); they are marked
as such where known.

New features start at 015 with `/speckit-specify`.
