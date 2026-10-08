# Contributing to SQMeter

Bug reports, feature requests, and pull requests are welcome.

---

## Reporting Issues

Use the [GitHub issue templates](https://github.com/DeanJ87/SQMeter/issues/new/choose):

- **Bug report** — something isn't working
- **Feature request** — something you'd like added

Include the firmware version (shown in the web UI System page), which sensors you have connected, and serial logs if relevant.

---

## Pull Requests

1. Fork the repo and create a branch from `main`
2. Make your changes (see code style below)
3. Test on real hardware if possible
4. Open a PR with a description of what and why

Keep PRs focused — one feature or fix per PR, targeting `main`.

---

## Spec-Driven Development

New features are built with [Spec Kit](https://github.com/github/spec-kit). The project's rules are in [`.specify/memory/constitution.md`](.specify/memory/constitution.md); every spec, plan and convergence check is measured against them. With Claude Code (or another supported agent) in the repo:

```text
/speckit-specify <what and why>
/speckit-plan <technical direction>
/speckit-tasks
/speckit-implement
/speckit-converge        # repeat implement -> converge until it reports converged
```

`/speckit-clarify`, `/speckit-analyze` and `/speckit-checklist` are optional quality gates. Specs live in `specs/<NNN-feature>/`. Bug fixes and small changes don't need a spec.

---

## Code Style

All code follows the [coding standard](docs/development/coding-standards.md) - naming, where code goes, smells to avoid, limits, comments, errors and tests. Every rule has an ID; CI checks the automatic ones:

```bash
python3 -m pip install -r tools/quality/requirements.txt   # once
python3 tools/quality/check.py          # what CI runs
python3 tools/quality/check.py --fix    # apply the formatters
```

Existing violations are in `tools/quality/baseline.json`; new ones fail. Fix one and run `--update-baseline` so the count goes down.

**Firmware build (zero warnings):**
```bash
pio run && pio test -e native
```

**Adding a sensor:** extend `SensorBase` - see [Adding Sensors](docs/development/sensors.md).

**Web UI dev server:**
```bash
cd web
ESP32_IP=<your-device-ip> npm run dev
```

---

## Commit Messages

Short imperative summary line, present tense:

```
Add MLX90614 cloud detection threshold config
Fix NTP sync dropping after WiFi reconnect
Update Bortle description strings
```

No trailing period. No "WIP" commits in PRs — squash before opening.

---

## Docs

Documentation lives in `docs/` and is built with [MkDocs Material](https://squidfunk.github.io/mkdocs-material/). Edit the relevant `.md` file and the site redeploys automatically on merge to `main`.

Preview locally:
```bash
pip install mkdocs-material
mkdocs serve
```
