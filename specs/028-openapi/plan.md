# Implementation Plan: OpenAPI description of the REST API

**Branch**: `feat/028-openapi` | **Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

## Summary

Generate one OpenAPI 3.1 document (`docs/api/openapi.json`) from the route registry, per-operation
details kept beside it, the contract JSON Schemas and demo-made examples; check it in CI against
the registry, the schemas and an OpenAPI validator; render it read-only on the docs site and as a
Scalar "Try it" explorer on the demo site, answered by the emulated device. No firmware change.

## Technical context

- Python 3 (stdlib) generator and check, `openapi-spec-validator` (pinned in
  `tools/quality/requirements.txt`) for validation.
- Web: Vite multi-page demo build (`web/explorer/index.html`, demo config only), Scalar
  `@scalar/api-reference` 1.72.1 (devDependency, exact).
- Docs: mkdocs-material, vendored Scalar standalone bundle (like mermaid).
- Constraints: no third-party requests (spec 016 FR-006, 024 FR-004), nothing on the device
  (FR-011, SIZE-01..03), docs a11y (022), relative/one-constant links (D6), no paid AI in CI.

## Constitution check

- Spec-driven, one PR, tests with the change, no firmware change, size budgets unchanged: pass.

## Structure

```text
tools/api/operations.json       per-operation summary, description, tags, parameters, bodies, responses
tools/api/openapi.py            generate (--out) and check (--check)
tools/api/test_openapi.py       unit tests (drift cases, SC-003)
tools/api/examples/*.json       demo-made examples
docs/api/openapi.json           the generated document (committed, checked)
docs/api/reference.md           read-only reference (vendored Scalar)
web/explorer/index.html         demo-only page
web/src/demo/explorer/*         boot the emulated device, mount Scalar, theme
web/scripts/vendor-scalar.mjs   copy the standalone bundle into the docs
specs/016-.../schemas/          + wifi-scan, releases schemas (FR-002)
```

## Phases

1. Schemas for routes without one (wifi scan, update releases), operation details, generator and
   check, committed document, CI wiring (quality gate + build workflow), release asset.
2. Examples from the demo, the docs reference page, rest.md link (FR-009).
3. Explorer on the demo site, themed; tests (loads, Try it, a11y, nothing leaves the browser).
4. Docs, changelog, convergence.
