# Implementation Plan: Coding Standards, Enforced

**Branch**: `spec/coding-standards` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

## Summary

Write one rule-ID'd coding standard (`docs/development/coding-standards.md`), make it binding through
a constitution amendment (new Principle VIII), configure one formatter and the linters per language,
add `tools/quality/check.py` that runs everything, adds the structure checks, and compares against a
committed baseline, wire it into CI, and reformat the codebase once.

## Technical Context

**Language/Version**: C++17 (firmware, ESP32 + native), TypeScript 5 (web), Python 3 (tools)

**Primary Dependencies**: clang-format 19, clang-tidy 19, lizard, ruff (pinned in
`tools/quality/requirements.txt`); ESLint 9 + typescript-eslint, eslint-plugin-react-hooks,
@eslint-community/eslint-plugin-eslint-comments, Prettier (web devDependencies)

**Storage**: `tools/quality/baseline.json` (committed)

**Testing**: `tools/quality/test_check.py` (unit tests for the baseline and custom rules); CI proves
each rule fails on a sample violation (SC-002)

**Target Platform**: GitHub Actions (ubuntu-latest) and contributors' machines

**Project Type**: Repository tooling + documentation

**Performance Goals**: full local check < 2 minutes (SC-005)

**Constraints**: no change to firmware behaviour (formatting only); generated code excluded

**Scale/Scope**: ~12k lines C++, ~8k lines TS, ~1k lines Python

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I-VI | No behaviour change; formatting-only commit verified by identical firmware builds | ✅ |
| III. Testable pure logic | Strengthened: structure rule STRUCT-01 enforces lib/ purity | ✅ |
| VII. Docs | Standard in docs; CONTRIBUTING links it | ✅ |
| Governance | Constitution amended to v1.1.0 (MINOR: new principle) with sync report | ✅ |

## Project Structure

```text
docs/development/coding-standards.md     # the standard (rule IDs)
.specify/memory/constitution.md          # Principle VIII + quality gate
.clang-format  .clang-tidy  .editorconfig  .git-blame-ignore-revs  pyproject.toml (ruff)
web/eslint.config.js  web/.prettierrc.json  web/.prettierignore
tools/quality/check.py  tools/quality/baseline.json  tools/quality/requirements.txt  tools/quality/test_check.py
.github/workflows/build.yml              # quality job
```

## Phases

1. Standard + constitution amendment.
2. Formatter configs; one formatting-only commit; blame-ignore.
3. Linter configs; `check.py` (runs tools + custom rules + baseline); unit tests.
4. Generate the baseline; CI job; sample-violation proof per rule.
5. CONTRIBUTING/docs; first convergence run against the standard (tasks appended to specs).
