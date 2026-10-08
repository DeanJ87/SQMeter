# Implementation Plan: Diagrams in the Docs

**Branch**: `spec/024-docs-diagrams` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/024-docs-diagrams/spec.md`

## Summary

Mermaid diagrams go into the docs site (sqmeter.dev):
- rendered by MkDocs Material's built-in support;
- styled by the site's palette;
- using a pinned mermaid served from the site itself, with no CDN.

Each diagram:
- is preceded by a metadata comment (ID, the sources it reflects, a blocking flag, a fingerprint);
- carries `accTitle`/`accDescr`, a caption and a "Diagram in words" block.

Two checks enforce this:
- `tools/docs/diagrams.py` (Python, no dependencies) checks completeness and freshness, and checks the built site for self-hosting.
- `web/scripts/check-diagrams.mjs` (Playwright + mermaid) parses and renders every diagram.

All 15 catalogue diagrams are drawn against the current code (P1–P3). PR #30's setup is reused and its four diagrams redrawn. See [research.md](research.md).

## Technical Context

**Language/Version**: Markdown (MkDocs Material 9.7.7), Mermaid 11.17.2, Python 3.12 (standard library), Node 24 (ESM script)
**Primary Dependencies**: mkdocs-material (pinned in `docs/requirements.txt`), pymdownx.superfences (bundled), mermaid (web devDependency), @playwright/test (existing)
**Storage**: N/A (fingerprints live in the Markdown metadata comments)
**Testing**:
- `python3 -m unittest tools/docs/test_diagrams.py` (parser, fingerprint, symbol extraction, the checks)
- `npm run docs:diagrams` (render every diagram)
- `mkdocs build --strict`
- a headless screenshot review
**Target Platform**: Static site on GitHub Pages (sqmeter.dev); GitHub's own Markdown rendering for the README and specs
**Project Type**: Documentation plus CI tooling
**Performance Goals**: The diagram pages load mermaid once (about 2.7 MB, cached); other pages load nothing extra
**Constraints**:
- No third-party runtime hosts (FR-004).
- Readable at 400 px (FR-002).
- No colour-only meaning (FR-003).
- The Pages deploy and the demo publish in `docs.yml` keep working.
**Scale/Scope**: 15 diagrams on about 12 pages; 2 checks; 1 new workflow

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Fail-safe safety verdict | Safety diagrams (DIA-02/03/04) are `blocking: true`. A change to the verdict code without re-confirming its diagram fails CI. The diagrams show that rain is evaluated regardless of stale data, and that unsafe is never delayed. ✅ |
| II. Alpaca conformance | DIA-10 documents the existing discovery, management and device API. No behaviour changes. ✅ |
| III. Testable pure logic | The check logic is plain Python with unit tests; the diagrams cite `lib/` pure-logic sources. ✅ |
| IV. Embedded budgets | Nothing ships to the device (diagrams are docs-site only). ✅ |
| V. Quiet, consistent UI | Diagrams use the site palette via Material's variables; no per-diagram colours. ✅ |
| VI. Trusted-LAN security | N/A. The docs site makes no new third-party requests (FR-004). ✅ |
| VII. Docs move with behaviour | This feature *is* that principle for diagrams. The freshness check makes drift visible, and the contributing rule (FR-009) requires updates. Where existing prose contradicts the code (research R6), the prose next to the diagram is corrected. ✅ |

There are no violations.

## Project Structure

### Documentation (this feature)

```text
specs/024-docs-diagrams/
├── spec.md
├── plan.md              # this file
├── research.md          # decisions R1–R8, code findings
├── data-model.md        # Diagram, SourceRef, Fingerprint, CheckResult
├── quickstart.md        # how to add / confirm / check a diagram
├── contracts/
│   └── diagram-block.md # the Markdown block format the checks enforce
└── tasks.md
```

### Source Code (repository root)

```text
mkdocs.yml                         # mermaid fence, custom_dir: overrides, nav
overrides/main.html                # loads vendored mermaid before the bundle, diagram pages only
docs/requirements.txt              # mkdocs-material==9.7.7
docs/stylesheets/sqmeter.css       # .mermaid scroll container, figure/caption styles
docs/assets/javascripts/vendor/    # mermaid.min.js copied at build (gitignored)
docs/**.md                         # the diagrams (catalogue in spec.md)
README.md                          # DIA-01 copy
tools/docs/diagrams.py             # completeness + freshness + --site + --confirm
tools/docs/test_diagrams.py        # unit tests
web/package.json                   # mermaid devDependency; docs:vendor, docs:diagrams scripts
web/scripts/vendor-mermaid.mjs
web/scripts/check-diagrams.mjs     # parse + render each block in Chromium
.github/workflows/diagrams.yml     # Python freshness/completeness check on source and docs changes
.github/workflows/docs.yml         # vendor, render check, --site check
CONTRIBUTING.md, docs/development/contributing.md  # FR-009 rule
```

**Structure decision**:
- The Python checks live in `tools/docs/`, beside the other repo tools.
- The browser-based render check lives in `web/scripts/` because it reuses the web toolchain (Playwright, node_modules) that the docs job already installs.

## Complexity Tracking

| Item | Why it's needed | Simpler alternative rejected because |
|---|---|---|
| Symbol-scoped sources (`path#symbol`) | Safety diagrams must block on safety-code changes, but their sources live partly in 1,000+ line files (`DeviceCore.cpp`) | Whole-file hashes would block every unrelated PR touching those files |
| Theme override for script loading | Material otherwise fetches mermaid from unpkg | `extra_javascript` loads after the bundle (too late), and on every page |
