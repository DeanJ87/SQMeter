# Research: OpenAPI description of the REST API

## D1 One committed document, generated

`tools/api/openapi.py` builds `docs/api/openapi.json` from the route registry
(`tools/api/routes.json`), the operation details beside it (`tools/api/operations.json`), the
contract schemas (`specs/016-demo-device-emulation/contracts/schemas/`) and demo examples
(`tools/api/examples/`). The output is committed so the docs site, the demo explorer and the tests
read one file, and `--check` fails when it's out of date (like the demo core's `SOURCE_HASH`).

- **Why committed**: the docs build, the demo build and vitest all need it; generating it in three
  places would mean three chances to differ.
- **Alternatives**: generate in each build (no stale file, but three generators); write the
  OpenAPI by hand (drifts - the reason for the spec).

## D2 Schemas copied into `components`, one source

OpenAPI 3.1 uses JSON Schema 2020-12. The contract schemas are draft-07 but only use `type`,
`properties`, `required`, `additionalProperties`, `items`, `enum`, `const`, `oneOf`, `not` and
`title`/`description`, which mean the same in 2020-12. The generator copies each one into
`components.schemas` (dropping `$schema`) and operations `$ref` them, so the contract tests and the
description use the same file. The partial-update body for `POST /api/config` is derived from the
config schema with every `required` removed.

## D3 Auth from the registry

`required` routes carry `security: [{deviceLogin: []}]` (HTTP Basic, as the device asks when
protection is on); `open` routes carry `security: []` and the registry's reason. Nothing is typed
twice.

## D4 Excluded by rule

Captive-portal probes (`$probe`), `/setup`, `/management` and `/api/v1` (ASCOM Alpaca) aren't
operations: Alpaca has its own published standard (FR-007), linked from `info` and the docs.

## D5 Explorer: Scalar, demo site only

- **Scalar** `@scalar/api-reference` 1.72.1 (pinned, published 2026-09-25), bundled by Vite into a
  separate demo page `/explorer/`. It's never part of the device build (FR-011): the device UI
  build has one entry, `index.html`, and the UI size check (SIZE-01..03) is unchanged.
- **"Try it"** requests go to the page's own origin, where the demo's mock service worker answers
  them with the emulated device (spec 016); the page starts the emulated device first, exactly as
  the demo app does. A second server entry, `http://{device}`, gives `curl` lines for a real SQMeter.
- **Nothing leaves the browser**: `telemetry: false`, `withDefaultFonts: false` (our own fonts),
  no proxy (`proxyUrl` unset), no "Open API client" button, no document download links to
  scalar.com; the demo's Content-Security-Policy still applies, and the existing test is extended
  to the explorer.
- **Theme**: the docs site and the device UI are dark only, so the explorer is dark only
  (`forceDarkModeState: 'dark'`, no toggle) with the UI's palette and fonts. The spec's "light/dark"
  becomes "matches the docs site", which has one scheme.
- **Docs reference (FR-005)**: the same Scalar bundle, vendored into the docs like mermaid
  (`web/scripts/vendor-scalar.mjs`, git-ignored), renders `docs/api/openapi.json` read-only on
  `docs/api/reference.md` with "Try it" hidden and a link to the explorer. Self-hosted, no CDN.
- **Rejected**: Swagger UI (dated), Redoc (no "Try it" in the free version), Stoplight Elements
  (stalled) - owner's decision, spec Clarifications.

## D6 Move-safe links

The explorer link in the docs is one constant (`extra.demo_url` in `mkdocs.yml`); the explorer
links back to the docs reference relatively where possible and otherwise through
`DOCS_URL` in `web/src/demo/links.ts`. When the docs move to docs.sqmeter.dev only those change.

## D7 Examples from the demo

`web/src/demo/__tests__/openapi-examples.test.ts` boots the emulated device, asks it the common
integrator questions (readings, verdict, a test alert, one setting change, status) and checks the
committed examples in `tools/api/examples/` still validate against their schemas;
`OPENAPI_EXAMPLES=write` refreshes them. They're true by construction and stay valid.

## D8 AsyncAPI (P3) deferred

Describing `/ws/*` and MQTT in AsyncAPI 3 isn't cheap to do well (two transports, topic templates)
and isn't needed for the P1 stories; it stays a tracked task (T020).

## D9 rest.md keeps the endpoint guide

FR-009 asks `docs/api/rest.md` to keep only what the generated reference can't carry. Its endpoint
sections stay: `tools/api/routes.py` requires every `/api/` route to be described there (spec 011
FR-006), and they carry behaviour the schemas can't (polling the WiFi scan, the restart after
joining, the safe-delay meaning of fields). The page now opens with links to the reference, the
explorer and `openapi.json`, and the reference links back for behaviour and auth.

## D10 Scalar's accessibility gaps, and what the pages fix

As loaded, Scalar 1.72.1 fails axe on two things the pages fix: unnamed icon buttons (copy code,
group toggles: named by a small observer) and 22 px sidebar toggles (24 px via CSS). The docs page
uses the `modern` layout because `classic` nests a copy button inside its accordion header
(nested-interactive). Scalar's request client (the "Try it" dialog) still has third-party issues
(ARIA on its key/value editor, some contrast); it's a tool panel opened on demand on the demo site
and the reference itself passes, so they're noted here rather than patched.

## D11 SC-004 in practice

The demo answers only inside a browser (its mock service worker), so a generated client can't call
it from outside. SC-004 is met in two halves: `openapi-python-client` generates a complete client
from `openapi.json` (all 32 operations, 12 GETs, no errors; checked during convergence), and
`web/tests/explorer.spec.ts` calls every documented GET route on the demo and expects JSON answers.
