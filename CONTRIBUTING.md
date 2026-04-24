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

Keep PRs focused — one feature or fix per PR.

---

## Code Style — Firmware (C++)

The project compiles with `-Wall -Wextra -Werror`. Your changes must build cleanly.

**Language standard:** C++17

**Key rules:**
- No stringly-typed code — use enums and structs, not `const char*` maps
- RAII resource management — no manual `new`/`delete`
- Const correctness — `const` on everything that shouldn't change
- No raw `String` (Arduino) in logic code — use `std::string`
- Prefer `std::optional` over sentinel values like `-1` or `nullptr`

**Adding a sensor:** Extend `SensorBase` — see [Adding Sensors](docs/development/sensors.md).

**Build and test:**
```bash
pio run          # must build with zero warnings
pio run --target upload && pio device monitor
```

---

## Code Style — Web UI (TypeScript / Preact)

- TypeScript strict mode — no `any`
- Define API response shapes with Zod schemas in `web/src/types/`
- Components in `web/src/`
- No external UI component libraries — Tailwind classes only

**Dev server:**
```bash
cd web
ESP32_IP=<device-ip> npm run dev
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
