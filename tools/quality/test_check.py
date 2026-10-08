"""Tests for the quality gate (spec 017 T010, T014).

The baseline logic is unit-tested, and every automatically enforced rule is
proved by a deliberately broken sample that must be reported under its rule
ID. Run: python3 -m unittest tools/quality/test_check.py (with the pinned
tools installed; the clang-tidy proof needs `pio pkg install -e native`).
"""

from __future__ import annotations

import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import check  # noqa: E402 - imported after the path is set up
import checks  # noqa: E402 - imported after the path is set up

SAMPLES = checks.ROOT / "tools" / "quality" / ".samples"
SAMPLE_SCOPE = "tools/quality/.samples"
WEB_SAMPLES = checks.WEB / "src" / "components" / "__quality_samples__"
HAS_JSON_LIB = any((checks.ROOT / ".pio" / "libdeps" / "native").glob("ArduinoJson*/src"))


def rules(findings: list[checks.Finding]) -> set[str]:
    return {f.rule for f in findings}


def web_path(path: str) -> str:
    return path.removeprefix("web/")


class BaselineTest(unittest.TestCase):
    def test_counts_per_rule_per_file(self):
        findings = [
            checks.Finding("LIMIT-01", "a.cpp", 1, ""),
            checks.Finding("LIMIT-01", "a.cpp", 9, ""),
            checks.Finding("FMT-01", "b.ts", 1, ""),
        ]
        self.assertEqual(check.count(findings), {"FMT-01": {"b.ts": 1}, "LIMIT-01": {"a.cpp": 2}})

    def test_new_rule_or_file_is_over(self):
        over, _ = check.compare({"LIMIT-01": {"a.cpp": 1}}, {})
        self.assertEqual(over, [("LIMIT-01", "a.cpp", 1, 0)])

    def test_increase_is_over(self):
        over, _ = check.compare({"LIMIT-01": {"a.cpp": 3}}, {"LIMIT-01": {"a.cpp": 2}})
        self.assertEqual(over, [("LIMIT-01", "a.cpp", 3, 2)])

    def test_same_count_passes(self):
        self.assertEqual(check.compare({"LIMIT-01": {"a.cpp": 2}}, {"LIMIT-01": {"a.cpp": 2}}), ([], []))

    def test_burn_down_is_reported_not_failed(self):
        over, under = check.compare({}, {"LIMIT-01": {"a.cpp": 2}})
        self.assertEqual((over, under), ([], [("LIMIT-01", "a.cpp", 0, 2)]))


class MappingTest(unittest.TestCase):
    def test_clang_tidy_naming(self):
        naming = "readability-identifier-naming"
        self.assertEqual(checks.tidy_rule(naming, "invalid case style for class 'foo'"), "NAME-01")
        self.assertEqual(checks.tidy_rule(naming, "invalid case style for enum constant 'FOO'"), "NAME-03")
        self.assertEqual(checks.tidy_rule(naming, "invalid case style for constexpr variable 'x'"), "NAME-03")
        self.assertEqual(checks.tidy_rule(naming, "invalid case style for local variable 'Bad'"), "NAME-02")
        self.assertEqual(checks.tidy_rule("bugprone-integer-division", "result of integer division"), "LINT-01")

    def test_ruff_codes(self):
        self.assertEqual(
            [checks.ruff_rule(c) for c in ("C901", "PLR0913", "N802", "F401", "S110", "B006")],
            ["LIMIT-02", "LIMIT-03", "NAME-09", "SMELL-11", "ERR-01", "LINT-01"],
        )


class SampleTest(unittest.TestCase):
    """Each automatic rule fails on a broken sample (T014)."""

    @classmethod
    def setUpClass(cls):
        SAMPLES.mkdir(exist_ok=True)
        WEB_SAMPLES.mkdir(exist_ok=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(SAMPLES, ignore_errors=True)
        shutil.rmtree(WEB_SAMPLES, ignore_errors=True)

    def write(self, directory: Path, name: str, text: str) -> str:
        path = directory / name
        path.write_text(text, encoding="utf-8")
        return checks.rel(str(path))

    # FMT-01 (FMT-02 is the formatter configuration itself)
    def test_unformatted_cpp(self):
        path = self.write(SAMPLES, "fmt.cpp", "int  main( ){return 0;}\n")
        self.assertEqual(rules(checks.check_clang_format([path], False)), {"FMT-01"})

    def test_unformatted_ts(self):
        path = self.write(WEB_SAMPLES, "Fmt.ts", "export const x = {a:1,b:2}\n")
        out = checks.run([str(checks.WEB / "node_modules" / ".bin" / "prettier"), "--list-different", web_path(path)], cwd=checks.WEB)
        self.assertIn("Fmt.ts", out.stdout)

    def test_unformatted_python(self):
        self.write(SAMPLES, "fmt.py", "x=( 1,2 )\n")
        out = checks.run([checks.tool("ruff"), "format", "--check", str(SAMPLES)])
        self.assertIn("Would reformat", out.stdout)

    # STRUCT-01
    def test_lib_includes_hardware_header(self):
        path = self.write(SAMPLES, "pure.h", "#include <Arduino.h>\n")
        self.assertEqual(rules(checks.check_lib_purity([path], scope=SAMPLE_SCOPE)), {"STRUCT-01"})

    def test_lib_reads_the_clock(self):
        path = self.write(SAMPLES, "clock.cpp", "unsigned long now() { return millis(); }\n")
        self.assertEqual(rules(checks.check_lib_purity([path], scope=SAMPLE_SCOPE)), {"STRUCT-01"})

    # EXC-01
    def test_bare_nolint(self):
        path = self.write(SAMPLES, "nolint.cpp", "int x; // NOLINT\nint y; // NOLINT(bugprone-foo): reason given\n")
        found = checks.check_suppressions([path], [])
        self.assertEqual([(f.rule, f.line) for f in found], [("EXC-01", 1)])

    def test_bare_noqa(self):
        marker = "# no" + "qa"  # split so this file doesn't contain a bare one
        path = self.write(SAMPLES, "noqa.py", f"import os  {marker}\nimport sys  {marker}: F401 - re-exported\n")
        found = checks.check_suppressions([], [path])
        self.assertEqual([(f.rule, f.line) for f in found], [("EXC-01", 1)])

    def test_eslint_disable_without_reason(self):
        path = self.write(WEB_SAMPLES, "Exc.ts", "// eslint-disable-next-line no-console\nconsole.log(1);\n")
        self.assertIn("EXC-01", rules(checks.check_eslint(False, (web_path(path),))))

    # LIMIT-01..05
    def test_cpp_limits(self):
        body = "".join(f"    if (a == {i}) b++;\n" for i in range(70))
        path = self.write(SAMPLES, "limits.cpp", f"int big(int a, int b, int c, int d, int e, int f)\n{{\n{body}    return b;\n}}\n")
        self.assertEqual(rules(checks.check_lizard([path], [])), {"LIMIT-01", "LIMIT-02", "LIMIT-03"})

    def test_python_function_length_and_nesting(self):
        lines = "".join(f"    x = {i}\n" for i in range(65))
        nested = (
            "def deep(a):\n    if a:\n        for b in a:\n            while b:\n"
            "                with open(b) as f:\n                    if f:\n                        return 1\n"
        )
        path = self.write(SAMPLES, "limits.py", f"def long():\n{lines}    return x\n\n\n{nested}")
        self.assertEqual(rules(checks.check_lizard([], [path])), {"LIMIT-01"})
        self.assertEqual(rules(checks.python_nesting(path)), {"LIMIT-05"})

    def test_python_complexity_params_naming_errors(self):
        branches = "".join(f"    if a == {i}:\n        return {i}\n" for i in range(17))
        text = (
            f"def complex_one(a):\n{branches}    return 0\n\n\n"
            "def many(a, b, c, d, e, f):\n    return a\n\n\n"
            "def BadName():\n    try:\n        return 1\n    except Exception:\n        pass\n"
        )
        path = self.write(SAMPLES, "lint.py", text)
        found = rules(checks.check_ruff(False, path))
        self.assertTrue({"LIMIT-02", "LIMIT-03", "NAME-09", "ERR-01"} <= found, found)

    def test_file_length(self):
        path = self.write(SAMPLES, "long.cpp", "int x;\n" * 601)
        py = self.write(SAMPLES, "long.py", "x = 1\n" * 401)
        self.assertEqual([f.path for f in checks.check_file_lengths([path], [py])], [path, py])

    def test_ts_rules(self):
        params = ", ".join(f"p{i}: number" for i in range(6))
        text = (
            "import { demoDevice } from '../../demo/device';\n"
            "export const badFetch = () => fetch('/api/status');\n"
            "export const useIt = (x: any) => [x, demoDevice];\n"
            "export function many(" + params + ") { try { return p0; } catch (e) {} }\n"
            "const unused = 1;\n"
            "export const snake_case_thing = 1;\n"
        )
        path = self.write(WEB_SAMPLES, "Rules.tsx", text)
        found = rules(checks.check_eslint(False, (web_path(path),)))
        self.assertTrue({"STRUCT-04", "STRUCT-05", "SMELL-18", "LIMIT-03", "ERR-01", "SMELL-11", "NAME-05"} <= found, found)

    def test_ts_size_limits(self):
        branches = "".join(f"  if (a === {i}) return {i};\n" for i in range(70))
        nest = "  if (a) { if (a) { if (a) { if (a) { if (a) { return 1; } } } } }\n"
        filler = "".join(f"export const v{i} = {i};\n" for i in range(400))
        path = self.write(WEB_SAMPLES, "size.ts", f"export function big(a: number) {{\n{nest}{branches}  return 0;\n}}\n{filler}")
        found = rules(checks.check_eslint(False, (web_path(path),)))
        self.assertTrue({"LIMIT-01", "LIMIT-02", "LIMIT-04", "LIMIT-05"} <= found, found)

    # NAME-01..03 and LINT-01 (clang-tidy)
    @unittest.skipUnless(HAS_JSON_LIB, "needs `pio pkg install -e native`")
    def test_cpp_naming_and_lint(self):
        text = (
            "namespace SQM {\n"
            "struct bad_type { int Member = 0; };\n"
            "constexpr int lowerConstant = 1;\n"
            "int Bad_Function(int x) { if (x) { return 1; } else { return 2; } }\n"
            "}\n"
        )
        path = self.write(SAMPLES, "naming.cpp", text)
        found = rules(checks.check_clang_tidy([path], scope=SAMPLE_SCOPE))
        self.assertTrue({"NAME-01", "NAME-02", "NAME-03", "LINT-01"} <= found, found)


if __name__ == "__main__":
    unittest.main()
