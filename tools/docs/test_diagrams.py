"""Tests for tools/docs/diagrams.py (spec 024).

    python3 -m unittest discover -s tools/docs -p 'test_*.py'
"""

import tempfile
import textwrap
import unittest
from pathlib import Path

import diagrams

GOOD_BLOCK = textwrap.dedent(
    """\
    # Page

    <!-- diagram: DIA-02
    sources: lib/logic.cpp#evaluate
    blocking: {blocking}
    fingerprint: {fingerprint}
    -->
    <figure class="diagram" markdown>

    ```mermaid
    flowchart TB
        accTitle: Title
        accDescr: Description
        A --> B
    ```

    <figcaption>What it shows.</figcaption>
    </figure>

    ??? info "Diagram in words"

        1. A leads to B.
    """
)

LOGIC = textwrap.dedent(
    """\
    // a declaration first, which must be skipped
    bool evaluate(int a);

    void other() { evaluate(1); }

    bool evaluate(int a)
    {
        if (a > 0)
        {
            return true;
        }
        return false;
    }

    bool evaluateLater(int a) { return a; }
    """
)


class TempRepo:
    def __init__(self, files):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        for name, text in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

    def __enter__(self):
        return self.root

    def __exit__(self, *exc):
        self.dir.cleanup()


def page(blocking="false", fingerprint="unconfirmed", body=None):
    text = GOOD_BLOCK.format(blocking=blocking, fingerprint=fingerprint)
    if body is not None:
        text = text.replace("    A --> B\n", body)
    return text


class DefinitionSpanTest(unittest.TestCase):
    def test_finds_the_definition_not_the_declaration_or_call(self):
        span = diagrams._definition_span(LOGIC, "evaluate")
        self.assertTrue(span.startswith("bool evaluate(int a)\n{"))
        self.assertTrue(span.rstrip().endswith("}"))
        self.assertIn("return false;", span)
        self.assertNotIn("evaluateLater", span)

    def test_qualified_names(self):
        text = "bool Filter::update(bool raw) const\n{\n    return raw;\n}\n"
        self.assertIn("return raw;", diagrams._definition_span(text, "Filter::update"))

    def test_missing_symbol_is_an_error(self):
        with self.assertRaises(diagrams.SourceError):
            diagrams._definition_span(LOGIC, "nowhere")


class FingerprintTest(unittest.TestCase):
    def test_symbol_fingerprint_ignores_changes_elsewhere_in_the_file(self):
        with TempRepo({"lib/logic.cpp": LOGIC}) as root:
            before = diagrams.fingerprint(["lib/logic.cpp#evaluate"], root)
            (root / "lib/logic.cpp").write_text(LOGIC + "\nint unrelated() { return 1; }\n", encoding="utf-8")
            self.assertEqual(before, diagrams.fingerprint(["lib/logic.cpp#evaluate"], root))
            (root / "lib/logic.cpp").write_text(LOGIC.replace("return false;", "return a < 0;"), encoding="utf-8")
            self.assertNotEqual(before, diagrams.fingerprint(["lib/logic.cpp#evaluate"], root))

    def test_directories_in_git_count_only_tracked_files(self):
        import subprocess

        with TempRepo({"lib/a/x.cpp": "x"}) as root:
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(["git", "-C", str(root), "add", "lib/a/x.cpp"], check=True)
            before = diagrams.fingerprint(["lib/a/"], root)
            (root / "lib/a/build.o").write_text("untracked build output", encoding="utf-8")
            self.assertEqual(before, diagrams.fingerprint(["lib/a/"], root))
            (root / "lib/a/x.cpp").write_text("changed", encoding="utf-8")
            self.assertNotEqual(before, diagrams.fingerprint(["lib/a/"], root))

    def test_directories_and_missing_paths(self):
        with TempRepo({"lib/a/x.cpp": "x", "lib/a/y.h": "y"}) as root:
            self.assertEqual(16, len(diagrams.fingerprint(["lib/a/"], root)))
            with self.assertRaises(diagrams.SourceError):
                diagrams.fingerprint(["lib/missing.cpp"], root)


class CheckTest(unittest.TestCase):
    def run_check(self, files):
        with TempRepo(files) as root:
            found = [d for path in sorted(root.rglob("*.md")) for d in diagrams.parse_file(path)]
            # Report paths relative to the temp repo.
            original = diagrams.ROOT
            diagrams.ROOT = root
            try:
                return diagrams.check(found, root), found, root
            finally:
                diagrams.ROOT = original

    def test_complete_and_confirmed_is_ok(self):
        with TempRepo({"lib/logic.cpp": LOGIC}) as root:
            value = diagrams.fingerprint(["lib/logic.cpp#evaluate"], root)
        report, found, _ = self.run_check({"lib/logic.cpp": LOGIC, "docs/p.md": page(fingerprint=value)})
        self.assertEqual([], report.errors)
        self.assertEqual([], report.warnings)
        self.assertEqual(1, len(found))

    def test_stale_is_a_warning_unless_blocking(self):
        report, _, _ = self.run_check({"lib/logic.cpp": LOGIC, "docs/p.md": page(fingerprint="0000000000000000")})
        self.assertEqual([], report.errors)
        self.assertIn("stale", report.warnings[0][1])
        report, _, _ = self.run_check({"lib/logic.cpp": LOGIC, "docs/p.md": page(blocking="true", fingerprint="0000000000000000")})
        self.assertIn("blocks", report.errors[0][1])

    def test_incomplete_diagrams_fail(self):
        cases = {
            "no accDescr": page().replace("    accDescr: Description\n", ""),
            "no caption": page().replace("<figcaption>What it shows.</figcaption>\n", ""),
            "no words": page().replace('??? info "Diagram in words"\n\n    1. A leads to B.\n', ""),
            "no metadata": page().split("-->\n", 1)[1],
            "inline style": page(body="    A --> B\n    style A fill:#f00\n"),
        }
        for name, text in cases.items():
            with self.subTest(name):
                report, _, _ = self.run_check({"lib/logic.cpp": LOGIC, "docs/p.md": text})
                self.assertTrue(report.errors, name)

    def test_bad_source_and_copies_that_differ(self):
        report, _, _ = self.run_check({"lib/logic.cpp": LOGIC, "docs/p.md": page().replace("#evaluate", "#renamed")})
        self.assertIn("bad source", report.errors[0][1])
        report, _, _ = self.run_check(
            {"lib/logic.cpp": LOGIC, "docs/p.md": page(), "docs/q.md": page(body="    A --> C\n")}
        )
        self.assertTrue(any("different content" in message for _, message in report.errors))

    def test_confirm_round_trip(self):
        with TempRepo({"lib/logic.cpp": LOGIC, "docs/p.md": page(blocking="true")}) as root:
            original = diagrams.ROOT
            diagrams.ROOT = root
            try:
                found = diagrams.parse_file(root / "docs/p.md")
                self.assertEqual(0, diagrams.confirm(found, ["DIA-02"], root))
                found = diagrams.parse_file(root / "docs/p.md")
                report = diagrams.check(found, root)
            finally:
                diagrams.ROOT = original
            self.assertEqual([], report.errors)
            self.assertRegex(found[0].fingerprint, r"^[0-9a-f]{16}$")


class SiteTest(unittest.TestCase):
    def test_site_must_vendor_mermaid_and_avoid_cdns(self):
        with TempRepo({"index.html": '<div class="mermaid"></div><script src="assets/javascripts/vendor/mermaid.min.js"></script>'}) as root:
            self.assertTrue(any("missing" in p for p in diagrams.check_site(root)))
            (root / diagrams.VENDORED).parent.mkdir(parents=True)
            (root / diagrams.VENDORED).write_text("// mermaid", encoding="utf-8")
            self.assertEqual([], diagrams.check_site(root))
            (root / "other.html").write_text('<div class="mermaid"></div><script src="https://unpkg.com/mermaid@11"></script>')
            problems = diagrams.check_site(root)
            self.assertTrue(any("unpkg.com" in p for p in problems))
            self.assertTrue(any("doesn't load the vendored" in p for p in problems))


if __name__ == "__main__":
    unittest.main()
