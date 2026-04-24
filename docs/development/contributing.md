# Contributing

See [CONTRIBUTING.md](https://github.com/DeanJ87/SQMeter/blob/main/CONTRIBUTING.md) in the repo root for the full guide.

---

## Quick Reference

**Firmware build (must be zero warnings):**
```bash
pio run
```

**Web UI dev server:**
```bash
cd web
ESP32_IP=<device-ip> npm run dev
```

**Docs preview:**
```bash
pip install mkdocs-material
mkdocs serve
```

Open a PR from a branch — one feature or fix per PR. Keep commit messages short and imperative.
