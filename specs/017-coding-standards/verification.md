# Verification: Coding Standards, Enforced

How each automatic rule was proved to fail CI (SC-002, T014), and the checks
run before merging.

## Every automatic rule fails on a sample

`tools/quality/test_check.py` writes a deliberately broken sample for each
rule, runs the real tool on it and asserts the finding carries the rule ID.
It runs in CI before the gate itself (Quality workflow, job `quality`).

| Rule | Sample | Tool |
|---|---|---|
| FMT-01 (and FMT-02, the formatter config) | unformatted C++, TS, Python | clang-format, Prettier, ruff format |
| NAME-01, NAME-02, NAME-03, LINT-01 | `struct bad_type`, `Bad_Function`, `constexpr lowerConstant`, else after return | clang-tidy |
| NAME-05, STRUCT-04, STRUCT-05, SMELL-18, SMELL-11, ERR-01, LIMIT-03 | `snake_case_thing`, `fetch` in a component, import from `demo/`, `any`, unused variable, empty catch, 6 parameters | ESLint |
| LIMIT-01, LIMIT-02, LIMIT-04, LIMIT-05 (TS) | long, branchy, deep function in a 400+ line file | ESLint |
| EXC-01 | bare `eslint-disable`, bare `NOLINT`, bare `noqa` | ESLint, check.py |
| LIMIT-01..03 (C++) | 70-line, 70-branch, 6-parameter function | lizard |
| LIMIT-01, LIMIT-05 (Python) | 65-line function, 5 nested blocks | lizard, ast |
| LIMIT-02, LIMIT-03, NAME-09, ERR-01 (Python) | 17 branches, 6 parameters, `BadName`, `except: pass` | ruff |
| LIMIT-04 | 601-line C++, 401-line Python | check.py |
| STRUCT-01 | `#include <Arduino.h>` and `millis()` in lib/ | check.py |

Result: 22 tests, all pass.

## End to end on the real tree

- A new `lib/RainLogic/src/*.cpp` calling `millis()`: `FAILED ... STRUCT-01 ... reads the clock - pass the time in`.
- A baseline count one higher than reality (an unrecorded fix): `FAILED: the baseline must shrink with the fix (FR-013)`.
- The checker's own files over the limit (check.py at 470 lines) were caught by LIMIT-04 and split.

## Before merging

- `python3 tools/quality/check.py`: OK, 188 findings, none new; about 10 s on a laptop (SC-005).
- Firmware sizes unchanged by the formatting commits: esp32dev 1,432,453 B, esp32dev-ble 1,660,657 B.
- `pio test -e native`: 146 passed. Web: tsc clean, vitest 197 passed, demo build, Playwright demo tests pass.
- Demo core SOURCE_HASH current; `mkdocs build --strict` passes.

## Baseline at adoption (188 findings)

| Rule | Findings | Files |
|---|---|---|
| LIMIT-01 function length | 34 | 20 |
| LIMIT-02 complexity | 39 | 31 |
| LIMIT-03 parameters | 13 | 7 |
| LIMIT-04 file length | 11 | 11 |
| LIMIT-05 nesting | 3 | 2 |
| LINT-01 linter findings | 12 | 9 |
| NAME-02 C++ camelCase | 2 | 2 |
| NAME-03 constants / enum values | 39 | 5 |
| NAME-05 TS naming | 2 | 2 |
| SMELL-11 unused code | 1 | 1 |
| SMELL-18 `any` | 1 | 1 |
| STRUCT-04 fetch in components | 31 | 13 |
