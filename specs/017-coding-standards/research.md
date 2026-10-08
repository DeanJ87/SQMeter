# Research: Coding Standards, Enforced

## Measurements (2026-10-08, lizard)

| Area | Functions | > 60 lines | Complexity > 15 | > 5 params | Files over limit |
|---|---|---|---|---|---|
| Firmware (`src`, `include`, `lib`) | 492 | 36 | 20 | 13 | 4 (> 600 lines): WebServer.cpp 2074, ConfigModel.cpp 1242, RG15Sensor.cpp 1143, DeviceCore.cpp 714 |
| Web (`web/src`) | 313 | 4 (3 in tests) | 3 | 0 | 5 (> 400 lines): AlertsTab.tsx 588, types/index.ts 535, mocks/data.ts 437, configSchema.ts 424, Dashboard.tsx 405 |
| Tools | 53 | 2 | 2 | 0 | - |

Worst offenders: `Config::applyJson` (431 lines, complexity 154), `Config::validate` (295 / 176),
`Alpaca::Router::device` (199 / 81), `WebServer::createStatusJson` (190), `setupOTA` (181).

**Decision**: keep the spec's default limits; the baseline absorbs today's violations.

## Tools

| Need | Decision | Why / alternatives |
|---|---|---|
| Limits (length, complexity, params) for C++, TS and Python | **lizard** | One tool, same numbers for all three languages; clang-tidy's readability-function-size only covers C++ and needs a compile database for `src/` (ESP32 toolchain) |
| C++ formatting | **clang-format** (pinned via pip) | Standard; `.clang-format` tuned to the current style (4 spaces, braces on their own line, 140 columns) |
| C++ lint (naming, bugprone, performance) | **clang-tidy** on `lib/` and `test/` only | They build on the host with plain flags; `src/` needs the Xtensa toolchain headers - covered by `-Werror` builds, limits, structure checks and convergence |
| TypeScript lint | **ESLint** flat config + **typescript-eslint**, **react-hooks** rules, **eslint-comments** (justified suppressions) | Standard for TS; naming-convention, no-restricted-globals/imports for structure rules |
| TS/CSS formatting | **Prettier** | One formatter, settings matching current code (2 spaces, single quotes, 140 columns) |
| Python | **ruff** (lint + format) | One fast tool replacing flake8/black/isort |
| Structure rules (lib/ purity, fetch outside the data layer, suppression reasons, file length) | **tools/quality/check.py** custom checks | Simple greps with rule IDs; no tool does them out of the box |
| Baseline | **tools/quality/baseline.json**: counts per rule per file, written by `check.py --update-baseline` | Counts (not line numbers) survive unrelated edits; per-file counts stop a fix in one file hiding a regression in another |

## Formatting all existing code (FR-016)

**Decision**: one commit that only runs the formatters, listed in `.git-blame-ignore-revs`
(GitHub honours it in blame views; locally `git config blame.ignoreRevsFile .git-blame-ignore-revs`).

## Speed (SC-005)

clang-tidy is the slow part; limited to `lib/` + `test/` (~40 files) it runs in about a minute; the
rest take seconds. `check.py --fast` skips clang-tidy for quick local runs; CI always runs everything.
