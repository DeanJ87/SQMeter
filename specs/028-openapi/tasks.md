# Tasks: OpenAPI description of the REST API

## Phase 1: Document and checks (US1, US2)

- [x] T001 [US1] Add contract schemas `wifi-scan.schema.json` and `releases.schema.json` in `specs/016-demo-device-emulation/contracts/schemas/` (FR-002) and validate the demo's answers against them in `web/src/demo/__tests__/contracts.test.ts`
- [x] T002 [US1] Write `tools/api/operations.json`: summary, description, tag, parameters, request body and responses for every `/api/*` registry route (FR-001, FR-003)
- [x] T003 [US1] Write `tools/api/openapi.py` (`--out`, `--check`, `--version`): registry + operations + schemas + examples → OpenAPI 3.1, exclusions by rule with reasons (FR-001..004, FR-007)
- [x] T004 [US2] Unit tests `tools/api/test_openapi.py`: route missing, extra operation, method drift, auth drift, unknown schema, stale document (SC-003)
- [x] T005 [US1] Generate and commit `docs/api/openapi.json`; validate with `openapi-spec-validator` (SC-002)
- [x] T006 [US2] Wire `openapi.py --check` into `tools/quality/check.py` and CI; pin the validator in `tools/quality/requirements.txt`
- [x] T007 [US1] Publish the document per release (`sqmeter-openapi-<tag>.json` asset, version stamped) in `.github/workflows/build.yml`

## Phase 2: Examples and docs reference (US1)

- [x] T008 [US1] `web/src/demo/__tests__/openapi-examples.test.ts`: make and check `tools/api/examples/` from the demo (FR-006)
- [x] T009 [US1] `web/scripts/vendor-scalar.mjs` + `docs:vendor`; `docs/api/reference.md` renders the document read-only, links to the explorer (FR-005)
- [x] T010 [US1] `docs/api/rest.md` links to the reference and keeps behaviour, auth and limits (FR-009); mkdocs nav

## Phase 3: Explorer (FR-010, FR-011)

- [x] T011 Demo page `web/explorer/index.html` + `web/src/demo/explorer/`: start the emulated device, mount Scalar with the demo origin and a real-device server, no telemetry, own fonts
- [x] T012 Theme the explorer like the docs site (dark, palette, fonts)
- [x] T013 Playwright `web/tests/explorer.spec.ts`: loads, themed, "Try it" on several operations returns the documented shape, a11y (axe), no request leaves the page
- [x] T014 Confirm nothing ships to the device: device build has no explorer/OpenAPI, UI size check unchanged

## Phase 4: Polish

- [x] T015 Docs: demo page mentions the explorer; integrations page links the reference; CHANGELOG
- [x] T016 Docs build strict, docs a11y on the reference page, no third-party requests from the docs
- [x] T017 Converge against spec.md: FR-001..007, 009..011 met; FR-008 (P3) deferred as T020; SC-001..003 by `tools/api/openapi.py --check` and `test_openapi.py`; SC-004 per research D11; quality gate rule API-01 added

## Deferred

- [ ] T020 [US3] AsyncAPI 3 for `/ws/*` and MQTT (FR-008, P3) - see research D8
