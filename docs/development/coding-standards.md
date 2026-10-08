# Coding Standards

How SQMeter code is written - firmware (C++), web UI (TypeScript/Preact) and tools (Python). Every rule has an ID so CI, reviews and `/speckit-converge` can cite it. The [constitution](https://github.com/DeanJ87/SQMeter/blob/main/.specify/memory/constitution.md) (Principle VIII) makes this standard binding.

**Check types**: **auto** - `tools/quality/check.py` fails CI; **converge** - checked by `/speckit-converge` and review, because a tool can't judge it.

Set up once:

```bash
python3 -m venv .venv-quality && .venv-quality/bin/pip install -r tools/quality/requirements.txt   # pinned tools
(cd web && npm ci)                  # ESLint and Prettier
pio pkg install -e native           # ArduinoJson, for clang-tidy
```

Then, with that venv's Python:

```bash
.venv-quality/bin/python tools/quality/check.py                    # everything CI runs
.venv-quality/bin/python tools/quality/check.py --fast             # without clang-tidy, for quick local runs
.venv-quality/bin/python tools/quality/check.py --fix              # linter autofixes, then the formatters, then the check
.venv-quality/bin/python tools/quality/check.py --update-baseline  # record burned-down findings
.venv-quality/bin/python -m unittest tools/quality/test_check.py   # the gate's own tests
```

In the web UI, `npm run lint`, `npm run format` and `npm run format:check` run ESLint and Prettier on their own.

CI runs the same in the **Quality** workflow (job `quality`) on every pull request. Each failure names the rule ID, the file and line, and how to fix it.

**Baseline.** Existing violations are counted per rule per file in `tools/quality/baseline.json`. A rule/file pair over its count fails, so new code meets the standard while old code is burned down. When you fix some, CI fails until you run `--update-baseline` in that PR, so the count goes down and can't creep back up. Formatting has no baseline: it's always clean. Never raise a count to get a change through - fix it, or make a one-place exception (EXC-01) with a reason the reviewer can judge.

**Not checked**: generated or vendored code - `web/src/demo/core/` (built device core), `web/public/mockServiceWorker.js`, `.pio/`, `node_modules/`, `web/dist*`, `site/`.

---

## Naming (NAME)

| ID | Rule | Check |
|---|---|---|
| NAME-01 | C++ types, classes, structs, enums and namespaces: `PascalCase` (namespaces under `SQM`). | auto (lib/, test/) |
| NAME-02 | C++ functions, methods, variables, parameters and members: `camelCase`, no `m_`/`_` prefixes or suffixes. | auto (lib/, test/) |
| NAME-03 | C++ `constexpr`/`static const` constants: `UPPER_SNAKE_CASE`. Enum values: `PascalCase` (older `UPPER_SNAKE` enums are in the baseline). | auto (lib/, test/) |
| NAME-04 | C++ files: `PascalCase.cpp/.h`, named after their main type; one main type per header. | converge |
| NAME-05 | TypeScript: components and types `PascalCase`; hooks `useThing`; functions and variables `camelCase`; module-level constants `UPPER_SNAKE_CASE`. Component files are named after the component. | auto |
| NAME-06 | JSON and API fields, settings keys: `camelCase` (spec 013). Names fixed by an outside protocol (Alpaca's `ClientTransactionID`, GitHub's `tag_name`, Home Assistant discovery keys) are kept at that boundary only. | converge |
| NAME-07 | MQTT topics: lowercase words separated by `/`, e.g. `<base>/alerts/armed`. | converge |
| NAME-08 | CSS classes: `kebab-case`, prefixed by the component or area (`wifi-option`, `demo-panel`). | converge |
| NAME-09 | Python: `snake_case` functions/variables, `PascalCase` classes, `UPPER_SNAKE_CASE` constants (PEP 8). | auto |
| NAME-10 | Tests say what they prove: C++ `test_<behaviour>_<condition>`, TS `it('does X when Y')`. | converge |
| NAME-11 | Names say what something is in this domain (`rainClearDelayMs`, `skyQuality`), with units in the name when it's a number with a unit (`Ms`, `C`, `Percent`, `Hpa`). No `data`, `info`, `temp`, `val` on their own. | converge |

```cpp
// Good
constexpr uint32_t WIND_STALE_MS = 5000;
bool windFresh(const WindReading &wind, uint32_t nowMs);
// Bad
#define windStale 5000
bool check(const WindReading &w, uint32_t t);
```

---

## Structure and domains (STRUCT)

| ID | Rule | Check |
|---|---|---|
| STRUCT-01 | `lib/` is pure: no Arduino, ESP-IDF, FreeRTOS, sensor-driver or networking headers, no `millis()`/`time()` - time and hardware state come in as parameters. That's what lets the native tests and the browser demo run it. | auto |
| STRUCT-02 | Decisions (what to report, whether it's safe, whether to alert, how to format a document) live in `lib/`; `src/` reads hardware, talks to the network and calls `lib/`. A decision in `src/` is a convergence finding. | converge |
| STRUCT-03 | One responsibility per module: a file that routes HTTP, owns sensor state and formats documents is three files. File-length limits (LIMIT-04) are the tripwire. | converge |
| STRUCT-04 | Web components render; data comes from the data layer (`web/src/hooks/`, `web/src/lib/`). Components don't call `fetch`, `WebSocket` or `XMLHttpRequest` directly. | auto |
| STRUCT-05 | Dependencies point inwards. Firmware: `src/` → `lib/`, never the reverse. Web: `components` → `hooks`/`lib` → `types`; only `main.tsx` and tests import from `mocks/` or `demo/`. | auto (web), converge (firmware) |
| STRUCT-06 | Generated code is never edited by hand; change its source and regenerate (e.g. `tools/demo-core/build.sh`). | converge |
| STRUCT-07 | One implementation of each rule. Logic needed by the firmware and the web/demo lives once (in `lib/`, compiled for the demo) - never re-written in TypeScript. | converge |

---

## Smells and anti-patterns (SMELL)

| ID | Smell | Why it hurts here | Preferred fix | Check |
|---|---|---|---|---|
| SMELL-01 | Long function | Hard to test; the ESP32's 8 KB loop stack makes deep, long functions risky | Extract named steps | auto (LIMIT-01) |
| SMELL-02 | God file / class | `WebServer.cpp` grew to 2,700 lines before spec 016 split it | Split by responsibility | auto (LIMIT-04) |
| SMELL-03 | Duplicated logic | The demo and firmware drifted apart when logic was copied | One implementation (STRUCT-07) | converge |
| SMELL-04 | Magic numbers | `30000` in three places means three meanings | Named constant with unit | converge |
| SMELL-05 | Stringly-typed code | Typos compile; `"stale"` vs `"Stale"` | `enum class` / TS union types | converge |
| SMELL-06 | Boolean parameter trap | `update(true, false)` is unreadable | Enum or options struct | converge |
| SMELL-07 | Deep nesting | Hides the main path | Early returns | auto (LIMIT-05) |
| SMELL-08 | Swallowed errors | Silent failures are how alerts went missing | Handle, return or log with context (ERR-01) | auto (empty catch), converge |
| SMELL-09 | Blocking in async handlers | Stalls the web server and WebSockets (Constitution IV) | Queue to a task or the main loop | converge |
| SMELL-10 | Heap churn in hot paths | Fragments the ESP32 heap over days of uptime | Reuse buffers; reserve; avoid temporary `String`s per tick | converge |
| SMELL-11 | Dead code | Misleads readers and the next refactor | Delete it | auto (TS/Python unused), converge |
| SMELL-12 | Commented-out code | Git remembers | Delete it | converge |
| SMELL-13 | Copy-pasted handlers | Fixes land in one copy | Shared helper or table-driven routes | converge |
| SMELL-14 | Untested decision logic | Constitution III | Move to `lib/` with native tests | converge |
| SMELL-15 | Mutable globals | Hidden coupling between tasks | Owned state passed explicitly; justified exception for ISR state | converge |
| SMELL-16 | Mixed or missing units | `windSpeed` m/s or km/h? | Units in names (NAME-11), metric internally | converge |
| SMELL-17 | Arduino `String` in logic | Heap churn, non-portable | `std::string` (outside thin Arduino glue) | converge |
| SMELL-18 | `any` / unchecked casts in TS | Defeats the type checker | Proper types, `unknown` + narrowing | auto |

---

## Limits (LIMIT)

| ID | Limit | C++ | TypeScript | Python | Check |
|---|---|---|---|---|---|
| LIMIT-01 | Lines per function | 60 | 60 (components 250) | 60 | auto |
| LIMIT-02 | Cyclomatic complexity per function | 15 | 15 | 15 | auto |
| LIMIT-03 | Parameters per function | 5 | 5 | 5 | auto |
| LIMIT-04 | Lines per file | 600 | 400 | 400 | auto |
| LIMIT-05 | Nesting depth | 4 | 4 | 4 | auto (TS, Python), converge (C++) |

Test files are exempt from LIMIT-01 (a `describe` block is long by nature) but not LIMIT-04.

---

## Formatting (FMT)

| ID | Rule | Check |
|---|---|---|
| FMT-01 | Formatted by the committed formatter: clang-format (C++), Prettier (TS/CSS/JSON), ruff format (Python). Don't hand-format. | auto |
| FMT-02 | 4-space indent in C++ with braces on their own line; 2 spaces in TS and CSS; 140-column lines. | auto |

---

## Comments (CMT)

| ID | Rule | Check |
|---|---|---|
| CMT-01 | Comments explain *why* - a decision, a constraint, a hardware quirk, a spec reference - not *what* the next line does. | converge |
| CMT-02 | Every public `lib/` function and type has a one-line purpose comment in its header. | converge |
| CMT-03 | Link the spec or issue a non-obvious behaviour comes from (`// spec 013 FR-004`). | converge |

---

## Errors and logging (ERR, LOG)

| ID | Rule | Check |
|---|---|---|
| ERR-01 | No silent failure: every caught error or failed call is handled, returned to the caller, or logged with what failed and why. | auto (empty catch), converge |
| ERR-02 | User-facing messages are plain and specific, and say how to fix it: "The rain sensor is switched off (Settings → Sensors → Rain sensor)", not "Error 3". | converge |
| ERR-03 | API errors use the REST contract: 4xx/5xx with `{"error": "..."}` (spec 013). | converge |
| ERR-04 | Fail safe: when a safety input can't be read, the verdict is unsafe (Constitution I). | converge |
| LOG-01 | Levels: `error` - something failed and needs attention; `warn` - degraded but coping; `info` - state changes worth knowing (connected, unsafe→safe); `debug` - detail for bring-up. No logging in per-reading hot paths above `debug`. | converge |
| LOG-02 | Never log secrets (passwords, keys, tokens, passkeys). | converge |

---

## Tests (TEST)

| ID | Rule | Check |
|---|---|---|
| TEST-01 | All `lib/` decision logic has native tests, including the boundaries (`>=` vs `>`). | converge |
| TEST-02 | A bug fix adds a test that fails without the fix (Constitution III). | converge |
| TEST-03 | Tests don't sleep for timing or touch the network; inject time and data. | converge |
| TEST-04 | Test names follow NAME-10. | converge |

---

## Linters (LINT)

| ID | Rule | Check |
|---|---|---|
| LINT-01 | Findings from the linters' bug and performance checks (clang-tidy `bugprone-*`, `performance-*`; ESLint and typescript-eslint recommended, React hooks; ruff `E`, `F`, `B`, `UP`, `SIM`) are fixed, or suppressed in one place with a reason (EXC-01). | auto |

---

## Exceptions (EXC)

| ID | Rule | Check |
|---|---|---|
| EXC-01 | A rule may be switched off for one place only, with the rule ID and a reason on the same line: `// NOLINT(readability-function-size): Alpaca router, one case per method` (C++), `// eslint-disable-next-line max-lines-per-function -- table of steps` (TS), `# noqa: C901 - parser for GitHub's format` (Python). Bare suppressions fail. | auto |

---

## Examples

Good and bad for the rules whose tables don't show one.

**STRUCT-03, SMELL-02 - one responsibility per module.** Bad: `WebServer.cpp` routes HTTP, builds the readings document and keeps the safety history. Good: routing in `src/WebServer*.cpp`, the document in `lib/Readings`, the history in `lib/SafetyHistoryLogic`.

**SMELL-01, LIMIT-01 - long function.** Bad: one 120-line `setupRoutes()`. Good: `setupStatusRoutes()`, `setupConfigRoutes()`, `setupAlpacaRoutes()`, each a screen long.

**SMELL-03 - duplicated logic.** Bad: the demo computing cloud cover in TypeScript. Good: the demo calling the firmware's `CloudDetection` through the WebAssembly core.

**SMELL-07, LIMIT-05 - deep nesting.**

```cpp
// Bad
if (cfg.rain.enabled) { if (reading.ok) { if (reading.rate > 0) { latch.set(now); } } }
// Good
if (!cfg.rain.enabled || !reading.ok || reading.rate <= 0)
    return;
latch.set(now);
```

**SMELL-08, ERR-01 - swallowed errors.**

```ts
// Bad
try { await saveConfig(cfg); } catch {}
// Good
try { await saveConfig(cfg); } catch (error) { toast.error(`Settings weren't saved: ${describe(error)}`); }
```

**SMELL-09 - blocking in async handlers.** Bad: an HTTP handler that waits for a Pushover request. Good: the handler queues the alert for the dispatcher task and returns 202.

**SMELL-11, SMELL-12 - dead and commented-out code.** Bad: `// oldCalibrate(raw);` left "just in case". Good: delete it; git has it.

**SMELL-13 - copy-pasted handlers.** Bad: six Alpaca `GET` handlers that differ only in the property. Good: one table of property names and getters.

**SMELL-15 - mutable globals.** Bad: `static float lastLux;` read by two tasks. Good: the value lives in the snapshot passed to whoever needs it.

**LIMIT-02 - complexity.** Bad: one function with a branch per alert event. Good: a table from event to level and wording.

**LIMIT-03 - parameters.** Bad: `evaluate(sqm, cloud, rain, wind, humidity, dewpoint)`. Good: `evaluate(const SafetyInputs &inputs)`.

**LIMIT-04 - file length.** Bad: a 1,000-line settings component. Good: one file per settings tab.

**FMT-01, FMT-02 - formatting.** Bad: hand-aligned code or a brace on the same line in C++. Good: whatever `check.py --fix` produces.

**CMT-01 - comments say why.** Bad: `// add one to count`. Good: `// The RG-15 counts the first tip twice after power-up; skip it.`

**ERR-02 - user-facing messages.** Bad: "Error 409". Good: "The rain sensor is switched off (Settings → Sensors → Rain sensor)".

**ERR-04 - fail safe.** Bad: no cloud reading, so the cloud rule is skipped and the verdict says safe. Good: no cloud reading, so the verdict is unsafe with "Cloud sensor not responding".

**LOG-02 - secrets.** Bad: `log("MQTT login %s:%s", user, password)`. Good: `log("MQTT login as %s", user)`.

**TEST-02 - a bug fix comes with a test.** Bad: fixing the release-list buffer size alone. Good: also adding a test with a release list longer than the old buffer, which fails without the fix.

**TEST-03 - no sleeping or network in tests.** Bad: `delay(15000)` to wait out the rain clear delay. Good: pass `now + 15000` to the latch.

**TEST-04 - test names.** Bad: `test1()`. Good: `test_latch_holds_for_clear_delay()`.
