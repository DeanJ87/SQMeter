# Tasks: Coding Standards, Enforced

## Phase 1: The standard (US1)

- [X] T001 [US1] Write docs/development/coding-standards.md: rules with IDs (NAME, STRUCT, SMELL, LIMIT, FMT, CMT, ERR, LOG, TEST, EXC), each with requirement, why, check type and a good/bad example; limits per research.md; exclusions list
- [X] T002 [US1] Amend .specify/memory/constitution.md to v1.1.0: Principle VIII "Code Quality Standards" (standard is binding, enforced by tools/quality/check.py with a shrinking baseline) and a quality gate; sync impact report
- [X] T003 [P] [US1] Replace CONTRIBUTING.md Code Style with a link to the standard and the check/fix commands; add the page to mkdocs.yml nav

## Phase 2: Formatting (US2, FR-010, FR-016)

- [X] T004 [P] .clang-format (C++), web/.prettierrc.json + .prettierignore, ruff format settings in pyproject.toml, .editorconfig - matching the current dominant style
- [X] T005 Reformat every file in one formatting-only commit; verify firmware binaries' size unchanged and all tests pass; record the commit in .git-blame-ignore-revs

## Phase 3: Linters and check.py (US2, US3, US5)

- [X] T006 [P] .clang-tidy (readability-identifier-naming per NAME rules, bugprone-*, performance-*, selected readability/modernize) for lib/ and test/
- [X] T007 [P] web/eslint.config.js: typescript-eslint, react-hooks, naming-convention, no-restricted-globals fetch/WebSocket in components (STRUCT-04), no-restricted-imports mocks/demo (STRUCT-05), eslint-comments require-description (EXC-01); npm scripts lint/format
- [X] T008 [P] ruff lint rules in pyproject.toml (E, F, B, UP, N, SIM, C90)
- [X] T009 tools/quality/check.py: runs formatters (check), clang-tidy, ESLint, ruff, lizard limits, file length, custom STRUCT/EXC checks; prints rule/file/line; compares per-rule-per-file counts to baseline.json; fails on any increase and on decreases not recorded (FR-013); formatting never baselined (FR-016); --update-baseline, --fix, --fast
- [X] T010 [P] tools/quality/test_check.py: baseline comparison and custom rules unit tests
- [X] T011 tools/quality/requirements.txt pinned versions

## Phase 4: Baseline and CI (US3, SC-002, SC-003)

- [X] T012 Generate tools/quality/baseline.json from today's code; commit
- [X] T013 CI job "quality" running check.py (all checks) - in its own workflow, .github/workflows/quality.yml, because build.yml has path filters and a required check must run on every PR
- [X] T014 Prove each automatic rule fails on a sample violation (scripted in test_check.py or a CI self-test)

## Phase 5: Convergence (US4)

- [x] T015 Run /speckit-converge against the standard; append findings (rule IDs + files) as tasks

## Dependencies

T001-T002 first (rules the tools implement); T004-T005 before T012 (baseline after formatting); T009 needs T006-T008.

## Phase 6: Convergence

- [X] T016 Bring the web UI's JSON under Prettier (check.py PRETTIER_GLOBS and npm format scripts; lockfile ignored) so FMT-01 holds for TS/CSS/JSON per FR-010 (partial)
- [X] T017 Add a concrete good/bad example for every rule that lacks one (STRUCT-03, SMELL-01/03/07/08/09/11/12/13/15, LIMIT-01..05, FMT-01/02, CMT-01, ERR-01/02/04, LOG-02, TEST-02..04) in docs/development/coding-standards.md per SC-001 (partial)
- [x] T018 Burn down LIMIT-04: split src/WebServer.cpp (2,219 lines), lib/ConfigModel/src/ConfigModel.cpp, lib/DeviceCore/src/DeviceCore.cpp, src/sensors/RG15Sensor.cpp, tools/demo-core/bridge.cpp, test/test_alpaca_logic/test_main.cpp, web/src/components/Dashboard.tsx, web/src/components/settings/AlertsTab.tsx, web/src/components/settings/SensorsTab.tsx, web/src/mocks/data.ts, web/src/types/index.ts per SC-006. The last one, tools/demo-core/bridge.cpp, is now the bindings only; EmulatedDevice lives in emulated_device.h with emulated_device.cpp and emulated_device_documents.cpp. Baseline: 0 findings
- [X] T019 Burn down STRUCT-04: move fetch/XMLHttpRequest out of Updates.tsx, AlertsTab.tsx, Settings.tsx, AlertsBell.tsx, Alpaca.tsx, System.tsx, WifiSetup.tsx, SensorsTab.tsx, Dashboard.tsx, SafetyCard.tsx, DeviceTab.tsx, NetworkTab.tsx, SafetyTab.tsx into web/src/hooks or web/src/lib per STRUCT-04 (partial)
- [X] T020 Burn down NAME-03: PascalCase enum values in lib/SensorTypes/include/SensorTypes.h, lib/AlpacaLogic/include/SafetyEvaluator.h, lib/BleLogic/include/BlePayloads.h, lib/BleLogic/include/BleAlarm.h; UPPER_SNAKE constants in lib/SkyLogic/src/Dewpoint.cpp per NAME-03 (partial)
- [X] T021 Burn down LIMIT-01..03: long, complex and many-parameter functions in src/WebServer.cpp, lib/ConfigModel/src/ConfigModel.cpp, lib/DeviceCore/src/DeviceCore.cpp, src/sensors/RG15Sensor.cpp, lib/AlertLogic/src/AlertEngine.cpp, lib/Readings/src/Readings.cpp, src/main.cpp and the rest listed in tools/quality/baseline.json per LIMIT-01, LIMIT-02, LIMIT-03 (partial)
- [X] T022 Burn down the small findings: NAME-02 (lib/AlertLogic/src/AlertEngine.cpp, lib/DeviceCore/src/DeviceCore.cpp), NAME-05 (web/src/components/__tests__/SkyCalibration.test.tsx, web/tests/screenshots.spec.ts), SMELL-11 (web/src/components/Updates.tsx), SMELL-18 (web/src/components/Settings.tsx), LIMIT-05 (web/src/hooks/useWifiScan.ts, tools/contract-check.py), LINT-01 (9 files in baseline.json) per NAME-02, NAME-05, SMELL-11, SMELL-18, LIMIT-05, LINT-01 (partial)
