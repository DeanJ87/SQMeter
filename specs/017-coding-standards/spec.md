# Feature Specification: Coding Standards, Enforced

**Feature Branch**: `spec/coding-standards`

**Created**: 2026-10-08

**Status**: Implemented (PR #89) - converged; baseline burn-down (T018-T022) open

> The audience is contributors, so the spec names the project's languages and areas
> (firmware C++, web TypeScript, Python tools) - they are the subject, not an
> implementation choice. Which tools enforce the rules is left to the plan.

## Clarifications

### Session 2026-10-08

- Q: Reformat existing code all at once, or file by file? → A: All at once, in one formatting-only
  commit that `git blame` is told to ignore.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - One written standard to code against (Priority: P1)

A contributor (human or AI) opens one document and knows how to name things, where code goes,
what's forbidden and why, for every part of the project.

**Why this priority**: Everything else - review, linting, convergence - checks against it.

**Independent Test**: Give the document to someone new; they can answer "where does this
logic go, what's it called, is this allowed?" for five sample changes without asking.

**Acceptance Scenarios**:

1. **Given** a change to the firmware, the web UI or a tool, **When** the contributor reads the
   standard, **Then** it states the naming, placement and limits that apply, with a short example.
2. **Given** a smell from the catalogue, **When** looked up, **Then** the standard says what it is,
   why it's harmful here, how to spot it, and the preferred fix.

---

### User Story 2 - Violations are caught automatically (Priority: P1)

Formatting, naming, unsafe patterns and limits are checked on every pull request; a new
violation fails the build with a message saying what and where.

**Why this priority**: A standard that isn't enforced drifts; review alone missed the 2,700-line
web server file and the duplicated logic spec 016 had to untangle.

**Independent Test**: Open a pull request that adds a 120-line function, an unformatted file and
a `fetch` inside a component; CI fails on each with a readable message.

**Acceptance Scenarios**:

1. **Given** a pull request, **When** it introduces a violation of an enforceable rule, **Then** CI
   fails and names the rule, file and line.
2. **Given** a contributor's machine, **When** they run one command, **Then** they get the same checks
   and can apply automatic fixes (formatting) locally.

---

### User Story 3 - Existing code is burned down, not a wall of failures (Priority: P1)

Turning enforcement on doesn't block all work on today's code: existing violations are recorded
in a baseline, new ones fail, and the baseline only ever shrinks.

**Why this priority**: ~23,000 lines of existing code can't all be fixed in one change.

**Independent Test**: Enable enforcement on the current code: CI passes; add one new violation:
CI fails; fix a baselined violation: the baseline count drops and CI requires the baseline update.

**Acceptance Scenarios**:

1. **Given** enforcement is introduced, **When** CI runs on unchanged code, **Then** it passes, with the
   existing violations listed in a committed baseline.
2. **Given** a change that fixes a baselined violation, **When** CI runs, **Then** the baseline must be
   updated to the lower count (it can't hide a regression elsewhere).
3. **Given** the baseline, **When** anyone looks at it, **Then** it shows counts per rule and per area,
   so progress is visible.

---

### User Story 4 - Convergence checks the code against the standard (Priority: P2)

`/speckit-converge` reports where the codebase departs from the standard - including the
judgement calls tools can't make (duplicated logic, wrong layer, swallowed errors) - and turns
them into tasks.

**Why this priority**: Gives a recurring, traceable clean-up loop instead of ad-hoc refactors.

**Independent Test**: Run convergence after the standard lands; it produces tasks that cite the
standard's rule IDs and specific files.

**Acceptance Scenarios**:

1. **Given** the constitution references the standard, **When** convergence runs, **Then** each finding
   cites a rule ID (e.g. `SMELL-04`) and a location.

---

### User Story 5 - Exceptions are deliberate and visible (Priority: P2)

When a rule genuinely doesn't fit (an interrupt handler, a generated file, a protocol-mandated
name), the contributor records a justified exception next to the code; unjustified exceptions fail.

**Why this priority**: Rules without an escape hatch get ignored wholesale.

**Independent Test**: Suppress a rule with and without a reason; only the version with a reason passes.

**Acceptance Scenarios**:

1. **Given** a suppression, **When** it has no reason or no rule ID, **Then** CI fails.
2. **Given** all suppressions, **When** listed, **Then** each shows rule, reason and location.

---

### Edge Cases

- Generated code (the demo's device core, mock service worker, vendored libraries) is excluded
  from all checks, listed explicitly.
- Names fixed by external protocols (ASCOM Alpaca's `ClientTransactionID`, GitHub API fields,
  Home Assistant discovery keys, ESP-IDF/Arduino APIs) are allowed where they cross the boundary.
- Hardware-timing code (interrupts, pulse counting) may need patterns otherwise flagged
  (e.g. volatile globals) - via a justified exception.
- A rule that produces many false positives is downgraded to a convergence-only (review) rule
  rather than disabled silently.
- Formatting existing files: one formatting-only commit (FR-016), so formatting never needs a baseline.

## Requirements *(mandatory)*

### Functional Requirements

**The standard**

- **FR-001**: The project MUST have one coding standard document covering firmware (C++), web UI
  (TypeScript/Preact), tools (Python) and shared conventions, with every rule given a stable ID
  (e.g. `NAME-03`, `STRUCT-02`, `SMELL-11`, `LIMIT-01`) so tools, reviews and convergence can cite it.
- **FR-002**: Each rule MUST state: what it requires, why (in this project's terms), how it's checked
  (automatic, convergence, or review), and a short good/bad example.
- **FR-003 Naming**: The standard MUST define naming per language for types, functions, variables,
  constants, enum values, files and directories, test cases, and shared conventions for JSON/API
  fields (camelCase), MQTT topics, settings keys and CSS classes - codifying the project's existing
  dominant style where one exists.
- **FR-004 Structure and domains**: The standard MUST define what belongs where: decision logic in
  `lib/` with no hardware/Arduino dependencies; hardware and I/O in `src/`; web components that
  render and a data layer that fetches; tools that are standalone. It MUST define allowed dependency
  directions and forbid the reverse (e.g. `lib/` including hardware headers; components calling the
  network directly).
- **FR-005 Smells**: The standard MUST include a catalogue of smells and anti-patterns, at least:
  long functions, god files/classes, duplicated logic (including between firmware, web and demo),
  magic numbers, stringly-typed code, boolean parameter traps, deep nesting, swallowed errors,
  blocking work in asynchronous handlers, heap churn in frequent paths, dead and commented-out code,
  copy-pasted handlers, untested decision logic, mutable globals, and inconsistent units.
- **FR-006 Limits**: The standard MUST set measurable limits - function length, file length,
  cyclomatic complexity, parameter count, nesting depth - per language, with the defaults in
  Assumptions.
- **FR-007 Comments**: Comments MUST explain why (decisions, constraints, hardware quirks), not
  restate the code; public library interfaces MUST have a one-line purpose.
- **FR-008 Errors and logging**: The standard MUST define how errors are reported (no silent
  failure: every caught error is handled, returned or logged with context), log levels and what each
  is for, and that user-facing messages are plain and specific (the device's error-message style).
- **FR-009 Tests**: The standard MUST define test naming, what must be tested (all `lib/` decision
  logic with boundary cases; every bug fix with a test that fails without it - Constitution III),
  and what tests must not do (sleep-based timing, network access).
- **FR-010 Formatting**: Each language MUST have exactly one automatic formatter configuration,
  committed, matching the existing style as closely as possible.

**Enforcement**

- **FR-011**: Every rule that can be checked automatically MUST be checked in CI on every pull
  request, failing with rule, file and line.
- **FR-012**: One local command MUST run the same checks, and one MUST apply automatic fixes.
- **FR-013**: Existing violations MUST be recorded in a committed baseline (counts per rule and
  area); CI MUST fail on any violation not in the baseline and MUST require the baseline to shrink
  when violations are fixed (it may never grow without an explicit, reviewed change).
- **FR-014**: Suppressions MUST name the rule and give a reason; CI MUST reject bare suppressions.
- **FR-015**: Generated and vendored code MUST be excluded by an explicit list.
- **FR-016**: The formatter MUST be applied to all existing files at once, in a single
  formatting-only commit with no other changes, listed in a blame-ignore file so `git blame` shows
  the real authors; from then on, formatting is checked like any other rule (no baseline for it).

**Governance**

- **FR-017**: The constitution MUST be amended to make the standard binding (a new principle or an
  expansion of the quality gates) so `/speckit-converge` checks code against it.
- **FR-018**: `CONTRIBUTING.md` and the developer docs MUST link the standard and the local
  commands, replacing the current short Code Style section.

### Key Entities

- **Rule**: ID, area, requirement, rationale, check type (automatic / convergence / review), example.
- **Baseline**: per rule and area, the count of known existing violations (and their locations).
- **Exception**: rule ID, location, reason.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of rules in the standard have an ID, rationale, check type and example.
- **SC-002**: Every automatically checkable rule is enforced in CI; a pull request introducing a
  new violation of each fails (verified by a sample violation per rule).
- **SC-003**: Enabling enforcement on the existing code produces a passing build with a baseline;
  the baseline's total count only goes down from then on.
- **SC-004**: The first convergence run against the standard produces tasks that each cite a rule
  ID and a file.
- **SC-005**: Running the full local check takes under 2 minutes on a typical laptop, so
  contributors actually run it.
- **SC-006**: Within three months of adoption, the largest files (today the 2,000+-line web server)
  are under the file-length limit or have a recorded, justified exception.

## Assumptions

- Default limits (adjustable in the plan with evidence):
  - Firmware C++: functions ≤ 60 lines, files ≤ 600 lines, cyclomatic complexity ≤ 15, ≤ 5
    parameters, nesting ≤ 4.
  - Web TypeScript: functions ≤ 60 lines, components ≤ 250 lines, files ≤ 400 lines, complexity ≤ 15.
  - Python tools: functions ≤ 60 lines, complexity ≤ 15.
- Naming codifies today's dominant style: C++ types `PascalCase`, functions/variables `camelCase`,
  constants `UPPER_SNAKE_CASE`, namespaces `PascalCase` under `SQM`; TypeScript components
  `PascalCase`, other identifiers `camelCase`; JSON/API camelCase (spec 013).
- Formatting codifies today's dominant style (C++ 4-space indent with braces on their own line;
  TypeScript 2-space, single quotes in components).
- Spec Kit's convergence reads the constitution; amending it is how the standard becomes checkable.

## Dependencies

- Constitution (amendment, FR-017); spec 013 (API naming); spec 016 (the demo's device core is
  generated and excluded).
