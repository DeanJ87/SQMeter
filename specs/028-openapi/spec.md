# Feature Specification: OpenAPI description of the device REST API

**Feature Branch**: `spec/028-openapi`

**Created**: 2026-10-10

**Status**: Draft, for review.

## Context

The device's REST API (`/api/*`) is described in three places that are each checked on their own:
the route registry (`tools/api/routes.json`: every route, method and whether it needs the password),
the JSON Schemas for the documents it returns (`specs/016-demo-device-emulation/contracts/schemas/`,
checked by the contract tests), and the prose reference (`docs/api/rest.md`). Integrators (Home
Assistant users, N.I.N.A. plug-in authors, script writers) have no machine-readable description they
can feed to a client generator, API explorer or test tool. This spec adds one OpenAPI 3.1 document,
built from the sources we already keep, so it can't drift from the device.

Out of scope: ASCOM Alpaca (`/api/v1/*`, `/management/*`, `/setup`) has its own published standard
and is linked, not re-described. MQTT and WebSocket are message streams; an AsyncAPI description of
them is optional (US3).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - An integrator reads the API in a standard format (Priority: P1)

A Home Assistant or script author opens sqmeter.dev, finds the API reference, and gets a browsable
reference and a downloadable `openapi.json` for the current release, which their tools (Swagger UI,
Postman, client generators) load without errors.

**Why this priority**: the reason for the feature.

**Independent Test**: load the published `openapi.json` in an OpenAPI 3.1 validator and in a client
generator; both succeed; every documented route answers on the demo with the documented shape.

**Acceptance Scenarios**:

1. **Given** the docs site for a release, **When** the user opens the API reference, **Then** every
   `/api/*` route in the route registry is listed with its method, parameters, auth requirement,
   request body (where any), success response and error responses.
2. **Given** `openapi.json`, **When** it is validated against OpenAPI 3.1, **Then** it is valid with
   no warnings.
3. **Given** a route that needs the password when protection is on, **Then** the description says so
   with a security requirement, and open routes say they are open.

---

### User Story 2 - The description can't drift from the device (Priority: P1)

A contributor adds or changes a route or a response field. CI fails until the OpenAPI document, the
route registry, the JSON Schemas and the code agree.

**Why this priority**: an out-of-date API description is worse than none.

**Independent Test**: add a route to `src/` without updating the registry → CI fails (already true);
add it to the registry without describing it → CI fails; change a schema field → the generated
OpenAPI changes and the docs build shows it.

**Acceptance Scenarios**:

1. **Given** a route in the registry with no OpenAPI operation, **Then** the check fails naming it.
2. **Given** an OpenAPI operation with no route in the registry, **Then** the check fails naming it.
3. **Given** a response document, **Then** its OpenAPI schema is the contract JSON Schema itself
   (referenced, not copied), so the contract tests and the description use one source.

---

### User Story 3 - Streams described too (Priority: P3)

An integrator wants the shape of `/ws/sensors`, `/ws/status` and the MQTT topics in a standard form.
An AsyncAPI 3 document describes them, reusing the same JSON Schemas.

**Independent Test**: the AsyncAPI document validates, and every topic and WebSocket message in
`docs/user-guide/mqtt.md` and `docs/api/websocket.md` appears in it.

---

### Edge Cases

- Upload routes (`/api/update`, `/api/update/fs`, `/api/i18n/upload`) are described as
  `multipart/form-data` with the file part, size limits and refusal responses (spec 027 layout checks).
- Captive-portal probes and Alpaca paths in the registry are excluded from the OpenAPI document by
  rule, and the check says why.
- Demo-only behaviour (simulated delivery, `demo: true` fields) is documented as such, not as device
  behaviour.
- Numbers and field names are locale-independent (spec 023): the description states it once.
- Versioning: `info.version` is the firmware version; breaking changes are listed in the changelog.

## Clarifications

### Explorer decisions (owner, 2026-10-10)

- Explorer on the demo site only. Not on the device (no space), and not on the docs site talking to
  a user's device (an https page can't call a plain-http LAN address; the device would need a
  trusted certificate).
- Scalar over Swagger UI (dated), Redoc (no "Try it" in the free version) and Stoplight Elements
  (stalled).
- Planned later, not part of this spec: sqmeter.dev becomes a project homepage and the docs move to
  docs.sqmeter.dev; links and the explorer must use paths that survive that move.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST publish one OpenAPI 3.1 document describing every `/api/*` route in the
  route registry, per release, at a stable URL on sqmeter.dev (latest) and as a release asset.
- **FR-002**: Request and response bodies MUST reference the contract JSON Schemas in
  `specs/016-demo-device-emulation/contracts/schemas/` (or their successor) rather than restating
  them; routes without a schema yet MUST get one.
- **FR-003**: Auth MUST come from the route registry: `required` routes carry a security requirement
  (HTTP Basic, as the device uses), `open` routes none, with the registry's reason in the description.
- **FR-004**: A CI check MUST fail when the registry, the OpenAPI operations and the schemas disagree
  (missing, extra or mismatched routes, methods or auth), naming each mismatch.
- **FR-005**: The docs site MUST render the reference from the document, self-hosted (no third-party
  CDN, spec 024 FR-004), readable at 400 px, and passing the docs accessibility checks (spec 022).
- **FR-006**: The description MUST include working examples for the common integrator tasks (read
  readings, read the verdict, test an alert, change one setting), taken from the demo so they stay
  true.
- **FR-007**: Alpaca MUST be linked to the ASCOM Alpaca API reference with the device's supported
  devices listed, not re-described.
- **FR-008** *(P3)*: An AsyncAPI 3 document SHOULD describe `/ws/*` messages and MQTT topics with the
  same schemas, checked against `docs/user-guide/mqtt.md` and `docs/api/websocket.md`.
- **FR-009**: `docs/api/rest.md` MUST link to the reference and keep only what the generated
  reference can't carry (behaviour, how auth works, limits).

### Key Entities

- **FR-010**: An interactive API explorer MUST be published on the demo site (demo.sqmeter.dev),
  generated from the same OpenAPI document: every operation browsable with a "Try it" request that
  the demo's emulated device answers in the browser (no hardware, nothing leaves the browser, spec
  016 FR-006), plus a copyable `curl` example for a real device. It uses Scalar, self-hosted and
  pinned (no third-party CDN at runtime), themed to match the docs site (colours, type, light/dark),
  and passes the docs accessibility checks (spec 022). The docs reference (FR-005) links to it.
- **FR-011**: The explorer MUST NOT ship on the device: no API explorer, Swagger UI or OpenAPI
  document in the device's filesystem (flash budget, spec 027 / SIZE-02).
- **Route registry**: routes, methods and auth (exists).
- **Contract schemas**: JSON Schemas of the documents (exist).
- **OpenAPI document**: generated from the two above plus per-operation descriptions and examples kept
  next to the registry.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of `/api/*` registry routes appear in the OpenAPI document; 0 extra operations.
- **SC-002**: The published document validates as OpenAPI 3.1 with 0 errors and 0 warnings.
- **SC-003**: A deliberate drift (route, method, auth or field) fails CI in each of the four cases.
- **SC-004**: A generated client (e.g. openapi-generator for Python) calls every GET route against the
  demo and parses the responses without manual fixes.

## Assumptions

- The registry stays the source of truth for routes; operation descriptions live beside it (extra
  fields in `routes.json` or a sibling file), not in C++ comments, so the firmware doesn't change.
- No firmware change is needed: the API exists; this documents it.
- The renderer is a pinned, self-hosted build (e.g. Redoc or Scalar standalone), vendored like
  mermaid; planning picks one by size and accessibility.
- AsyncAPI is optional and can ship after the OpenAPI document.
