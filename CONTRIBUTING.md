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
python3 -m venv .venv-quality && .venv-quality/bin/pip install -r tools/quality/requirements.txt   # once
(cd web && npm ci) && pio pkg install -e native                                                    # once
.venv-quality/bin/python tools/quality/check.py          # what CI runs (the "Quality" workflow)
.venv-quality/bin/python tools/quality/check.py --fast   # quicker: skips clang-tidy
.venv-quality/bin/python tools/quality/check.py --fix    # autofix and format, then check
```

Existing violations are in `tools/quality/baseline.json`; new ones fail. Fix one and run `--update-baseline` so the count goes down. See the [standard](docs/development/coding-standards.md) for details.

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
pip install -r docs/requirements.txt
(cd web && npm run docs:vendor)   # the docs serve their own copy of mermaid for diagrams
mkdocs serve
```

The page screenshots (`docs/assets/screenshots/`) aren't committed: CI makes them from the demo before building the site. For a local `mkdocs build --strict` without missing-image warnings, make them first with `(cd web && npm run build:demo && npm run screenshots)`.

### Diagrams

Diagrams are [Mermaid](https://mermaid.js.org/) in the Markdown, in the shape described in [`specs/024-docs-diagrams/contracts/diagram-block.md`](specs/024-docs-diagrams/contracts/diagram-block.md). Each one has:
- a comment naming the code it reflects;
- a fingerprint of that code;
- `accTitle`/`accDescr`, a caption, and a "Diagram in words" block.

**A change to behaviour shown in a diagram updates the diagram in the same pull request** (coding standard DOC-01).
1. Update the diagram, its caption and its words.
2. Record that it matches the code again:
   ```bash
   python3 tools/docs/diagrams.py --confirm DIA-NN
   ```

CI checks every diagram:
- **Completeness** is checked by `python3 tools/docs/diagrams.py`.
- **Parse and render** are checked by `cd web && npm run docs:diagrams`.
- **Self-hosting:** the built site must serve its own mermaid, never from a CDN.

When the code a diagram cites changes, the check flags the diagram as stale. For the safety-verdict diagrams (rules, safe delay, rain latch), that fails the build until the diagram is confirmed.

Cite the narrowest source you can, so unrelated edits don't flag the diagram: a header, or one function with `path#Function`.
