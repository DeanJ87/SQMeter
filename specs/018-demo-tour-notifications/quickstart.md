# Quickstart: Demo Tour and Real Notifications

```bash
pio test -e native -f test_device_core          # AlertDelivery requests
pio run -e esp32dev && pio run -e esp32dev-ble  # firmware builds; same requests as before
python3 tools/demo-core/source_hash.py --check  # demo core rebuilt
cd web && npx tsc --noEmit && npx vitest run && npm run build:demo
npx playwright test tests/demo-tour.spec.ts tests/demo-real-send.spec.ts tests/demo.spec.ts
```

Expected:

- Fresh browser: the tour is offered once; Start walks through the steps; "Do it for me" on the rain step
  turns the verdict unsafe and the step moves on; Esc ends it; "Take the tour" in the Demo panel restarts it.
- Default: "nothing leaves the browser" passes.
- Real notifications on with an ntfy topic, Send a test: one POST to `https://ntfy.sh/<topic>` with the
  device's Title/Priority/Tags; the alert list shows the ntfy result. A Pushover error shows its message.
  A second test within 30 s is skipped by the rate limit.
- After a reload, no key or topic is in localStorage or sessionStorage.
