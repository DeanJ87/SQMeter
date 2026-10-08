# Contract: Demo URLs

The demo is a static site at any origin and base path (today `https://demo.sqmeter.dev/`).

| URL | Result |
|---|---|
| `/` and `/#/<page>` | The web UI, as on the device |
| `/api/...`, `/management/...` (opened directly) | Served by `404.html`: the emulated device's response, pretty-printed, with its HTTP status |
| `/setup`, `/setup/v1/<type>/0/setup` | Redirect to `/#/settings?tab=safety`, as the device does |
| Anything else | The UI's Not Found page |

Requests the page makes (fetch, WebSocket) go to the emulated device only. The only network
requests are for the demo's own files (`connect-src 'self'`).
