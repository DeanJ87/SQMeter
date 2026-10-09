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

## Docs (DOC)

| ID | Rule | Check |
|---|---|---|
| DOC-01 | A change to behaviour shown in a docs diagram updates the diagram, its caption and its "Diagram in words" in the same PR, then re-confirms it with `python3 tools/docs/diagrams.py --confirm DIA-NN` (spec 024). | auto (stale diagrams warn; safety diagrams fail) |

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

## Accessibility (A11Y)

The web UI, the demo and the docs meet **WCAG 2.2 AA** (spec 022). These rules have their own checks rather than `check.py`:
- **a11y CI**: the axe page checks in the *Deploy Docs & Demo* workflow (`web/tests/a11y*.spec.ts`).
- **test**: a Vitest test.
- **converge** / **manual**: review, and the release checklist in [Accessibility (development)](accessibility.md).

| ID | Rule | Check |
|---|---|---|
| A11Y-01 | Text meets 4.5:1 (3:1 large); control edges, meaningful chart lines and focus indicators meet 3:1. Use the theme tokens: `--dim` is the faintest text colour, and `--control-edge` the edge of inputs. | a11y CI, test |
| A11Y-02 | Every interactive element shows the global `:focus-visible` ring. Never `outline: none` without an equal replacement. Sticky or floating UI must not cover the focused element. | a11y CI (partly), manual |
| A11Y-03 | Icon-only buttons and links have an accessible name (`ariaLabel`). Decorative SVG is `aria-hidden`. | a11y CI |
| A11Y-04 | Form controls are labelled. Put inputs in a `Field`, which labels them and ties its error to them (`aria-describedby`, `aria-invalid`), or pass `ariaLabel`. | a11y CI |
| A11Y-05 | Targets are at least 24 × 24 CSS px, or have equivalent spacing. | a11y CI (`target-size`), manual |
| A11Y-06 | Keep the skip link, one `h1`, the landmarks (`header`, `nav`, `main`) and `lang`. | a11y CI |
| A11Y-07 | Nothing appears only on hover. Use `InfoTip`: focusable, tappable, closes with Escape. | converge, manual |
| A11Y-08 | Content reflows at 320 px with no sideways page scroll; tables and charts scroll in their own box. | a11y CI, manual |
| A11Y-09 | Live data never announces itself. Only a verdict change, a new alert, and connection loss and recovery are announced, once, through `announce()` / `useAnnounceChange()`. Never put `aria-live` on an updating value. | a11y CI (`a11y-announce.spec.ts`), converge |
| A11Y-10 | Status is never colour alone: each state has text, or an icon with a text equivalent. | converge, manual |
| A11Y-11 | Charts and sparklines have a text alternative giving their meaning (`role="img"` with a label from `summariseSeries` or equivalent), and "no data yet" when empty. | a11y CI, converge |
| A11Y-12 | Dialogs and flyouts move focus in when opened, close on Escape and return focus to their trigger (`useDialogFocus`). Modal dialogs also keep Tab inside. | test, manual |
| A11Y-13 | Composite widgets follow the WAI-ARIA patterns: tabs (arrows, roving tabindex, labelled panel), switches (`role="switch"`), progress bars (`role="progressbar"` with a value). | test, converge |
| A11Y-14 | Motion respects `prefers-reduced-motion`. New animation goes in CSS, which the reduced-motion block covers; JS scrolling uses `scrollBehavior()`. | converge, manual |
| A11Y-15 | A new component comes with a test that finds it by role and accessible name, or its page is in `web/tests/a11y/inventory.ts`. | converge |

---

## Translations (I18N)

The web UI is translated (spec 023, [Translations](translations.md)). No baseline: every finding fails.

| ID | Rule | Check |
|---|---|---|
| I18N-01 | No user-facing text in the web UI outside `t()`: JSX text, user-facing attributes (`label`, `title`, `hint`, `aria-label`, ...), sentences in string literals and words joined to a value in a template (`` `rises ${clock}` ``). Put the English in `web/src/i18n/en.json` with a context note; build sentences with placeholders and plurals, never by joining pieces. Units and product names are exempt. A deliberate exception: `// i18n-ignore: <reason>`. | auto (`tools/i18n/literals.mjs`) |
| I18N-02 | Every language file has exactly the English keys, the same `{placeholders}`, the plural forms its language uses and English's edge spaces; every key has a context note; the device message templates and the notes are current. | auto (`tools/i18n/check.mjs`, `gen_device_catalog.py --check`, `context.py --check`) |
| I18N-03 | Layout uses logical CSS properties (`margin-inline-start`, `inset-inline-end`, `text-align: start`), so right-to-left languages mirror; readings, units, coordinates and charts stay left-to-right (`.ltr`, `.metric-value`). Directional arrows get `.dir-icon`. | Playwright (`web/tests/i18n.spec.ts`), converge |
| I18N-04 | No translated label is built when a module loads: call `t()` when rendering (or in a function), because the demo imports some modules before the language is loaded. | converge |
| I18N-05 | Numbers and dates follow the active language: format them with `web/src/i18n/format.ts` (`formatNumber`, `formatCount`, `formatBytes`, `formatCoordinates`, `formatTime`, `formatDateTime`) and read typed numbers with `parseNumber` (`web/src/i18n/parse.ts`), which takes the language's decimal separator and never truncates. No `toFixed`, `toPrecision`, `toLocale*String`, `parseFloat` or `parseInt` in components or the demo; SVG geometry uses `web/src/lib/svg.ts`, option values `Number(value)`. Digits are Latin in every language. | auto (ESLint `no-restricted-syntax`) |

---

## Dashboard (DASH)

The dashboard shows the device state that decides whether it is safe to observe and whether anyone will be told (spec 025). `web/src/dashboard/inventory.json` records, for every field the device reports and every settings dependency, where the dashboard shows it, or why it doesn't.

| ID | Rule | Check |
|---|---|---|
| DASH-01 | A change that adds device state a user would act on (a status, readings, safety or alerts field, or a settings dependency) updates the inventory: shown, with its visibility rule and a test in `web/tests/dashboard.spec.ts` that drives the demo into the state, or not shown with a reason. State that can block observing or silence alerts goes in the Status card (spec 026): a tile for Safety, Alerts, Data or the imaging app, otherwise one row. | converge |
| DASH-02 | Every contract-schema field and dependency id is mapped; every shown entry has a test and a translated label; no mapping is stale. | auto (`tools/dashboard/check.py`) |

---

## UI design system (DS)

The web UI is built from a small set of parts in `web/src/components/ui.tsx`, and new screens use them the way the existing ones do (spec 026). A UI change cites the DS rules it relies on, and a spec that adds or changes UI includes a mockup screenshot reviewed before it is built.

**Layout and parts**

| ID | Rule | Check |
|---|---|---|
| DS-01 | Dashboard information lives in cards built with `Card` (title, icon, optional "?" `hint`, optional `actions` pill). Nothing sits between the header and the card grid but the Arrange toolbar, and nothing is added to the header but its navigation and the alerts bell: no banners, strips or bars. | auto (`web/tests/layout-overlap.spec.ts`) |
| DS-02 | State is a `Pill` (green ok, amber attention, red problem, dim unknown). A card's overall state, if any, is one pill in its `actions`. | converge |
| DS-03 | Short values are `MetricTile`s in a `tile-grid`/`metric-grid`; label-value pairs are `ReadingRow`s. A new component needs a reason in the pull request; compose these first. | converge |
| DS-04 | Explanations, definitions and caveats go behind `InfoTip` ("?"): on a card title (`hint`), a field label or a row label. Text on the page is a one-line `Note` that reports a current state or an error, in the card it belongs to. | converge |
| DS-05 | Long values (IPv6, MAC, URLs, topics) are one value per row, monospace, left-to-right, truncated with the full value in `title`; never run together. | converge |
| DS-06 | A task's progress (update, upload, language download) shows in the card that started it: a status row with a pill, a `ProgressMeter`, then a `Note` with the outcome and at most one action (e.g. Retry). Not in the header, not on other pages. | converge |
| DS-07 | Confirmations ("Saved") are toasts; a lasting problem is a `Note` in its card and, if it affects observing, a row in the Status card. | converge |
| DS-08 | A problem is listed once, in the Status card; other cards show their own state but don't repeat global problems. A Status tile is its label, "?" and pill (plus a button for an action such as Resume): the why goes in "?". A sensor's fault card is its title and pill. | auto (`web/tests/dashboard.spec.ts`, "no run-on text") + converge |
| DS-09 | On a phone (≤ 599 px) cards stack in one column, tiles stay two per row, and nothing overlaps or hides a control (spec 022). | auto (`web/tests/layout-overlap.spec.ts`) |
| DS-10 | Spacing, colour and type come from the tokens in `web/src/index.css`; a new colour, size or shadow adds a token. | converge |

**Copy** (English is the source; translations follow it)

| ID | Rule | Check |
|---|---|---|
| DS-20 | Labels and titles are sentence-case nouns, three words where possible ("Local name", not "This device's local network name"); at most 32 characters. | auto (`tools/ui/copy_check.py`) |
| DS-21 | Pills and tile values are one or two words, at most 16 characters ("Safe", "Sending", "Live", "Connected"). | auto (`tools/ui/copy_check.py`) |
| DS-22 | A note is one sentence of at most 90 characters: the state first, then the fix. | auto (`tools/ui/copy_check.py`) |
| DS-23 | A "?" hint is at most two short sentences, 160 characters: what it is and why it matters. | auto (`tools/ui/copy_check.py`) |
| DS-24 | No run-on status lines joined with " · " or " - ": one fact per tile or row, the reason in "?". This includes text composed in code or sent by the device. | auto (`tools/ui/copy_check.py` for strings; `web/tests/dashboard.spec.ts` "no run-on text" for what every dashboard card renders) |
| DS-25 | No filler: "at a glance", "simply", "please note", "note that", "it is important", "in order to", "ensure", "seamless", "worked out in", "as soon as the device has it"; no parentheses that restate the label. Product names (N.I.N.A.) only as examples. | auto (`tools/ui/copy_check.py`) |
| DS-26 | Whose time or place it is (this browser's time zone, the device's location) is said once, in a hint, not on every line. | converge |
| DS-27 | One name for each thing in the UI, alerts, Home Assistant and the docs, from `tools/i18n/glossary/en.json`, including card titles and the safety reasons and alert titles the firmware sends. Part numbers are extra detail where hardware matters (System page rows, hardware docs, Alpaca sensor descriptions), never the name. | auto (`tools/ui/label_check.py`) |
| DS-28 | Docs name the UI as it is: every bold `Settings → Tab → Card → Control` path uses labels the UI shows (`web/src/i18n/en.json`, or a product-name card title like "ASCOM Alpaca"), so renaming a tab or card can't leave the docs pointing at something that's gone. | auto (`tools/docs/ui_paths.py`, DS-PATH) |

The type of each English string comes from its context note in `web/src/i18n/en.context.json` (`python3 tools/ui/copy_check.py --list`). A string that can't follow a rule is listed in `tools/ui/copy-exceptions.json` with its reason (EXC-01).

---

## Size (SIZE)

The device serves the web UI from a 512 KB LittleFS partition that also holds the chosen language file, and phones load it over the setup hotspot. The UI is stored gzipped (`tools/ui/pack_data.py`); `tools/ui/size_check.py` prints the sizes in CI.

| ID | Rule | Check |
|---|---|---|
| SIZE-01 | One feature or change grows the device UI's gzipped JS + CSS by at most 10 KB. The current size is recorded in `tools/ui/size-baseline.json`; a change that moves it by more than 2 KB records the new size (`python3 tools/ui/size_check.py --update-baseline`), and CI measures the growth against the record on the branch the pull request merges into. A feature that genuinely needs more records why (`--update-baseline --reason "..."`) and justifies it in its pull request. | auto (`tools/ui/size_check.py`) |
| SIZE-02 | The UI as stored, plus one language file at its 64 KB limit, plus LittleFS block overhead, uses at most 75% of the smallest LittleFS partition. | auto (`tools/ui/size_check.py`) |
| SIZE-03 | Text files go onto the device gzipped; demo-only files (the MSW service worker) never do. | auto (`tools/ui/test_ui_size.py`) |
| SIZE-04 | Each firmware build (`firmware.bin`, standard and Bluetooth) uses at most 95% of its app slot; CI warns above 90% and lists the largest contributors from the linker map. Sizes are recorded in `tools/firmware/size-baseline.json` (kept within 2 KB, `python3 tools/firmware/flash_budget.py --update-baseline`) and each pull request shows the change from its target branch. A build that genuinely needs more than 95% records why (`--update-baseline --reason "..."`). | auto (`tools/firmware/flash_budget.py`) |

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

**A11Y-04 - labelled inputs.** Bad: `<label class="field-label">Port</label><input type="number" />`, where nothing ties the two. Good: `<Field label="Port" error={error('mqtt.port')}><NumberInput ... /></Field>`; the input is named "Port" and described by its error.

**A11Y-09 - announcements.** Bad: `<div aria-live="polite">{sqm}</div>`, which reads every reading aloud. Good: `useAnnounceChange(safety.safe, (safe) => (safe ? 'Observatory safe' : 'Observatory unsafe'))`.

**A11Y-12 - dialogs.** Bad: a flyout that opens with focus left on the button behind it. Good: `useDialogFocus(open, flyoutRef, buttonRef)`, plus Escape to close.

**TEST-02 - a bug fix comes with a test.** Bad: fixing the release-list buffer size alone. Good: also adding a test with a release list longer than the old buffer, which fails without the fix.

**TEST-03 - no sleeping or network in tests.** Bad: `delay(15000)` to wait out the rain clear delay. Good: pass `now + 15000` to the latch.

**TEST-04 - test names.** Bad: `test1()`. Good: `test_latch_holds_for_clear_delay()`.
