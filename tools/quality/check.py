#!/usr/bin/env python3
"""SQMeter quality gate: the automatic rules of the coding standard.

docs/development/coding-standards.md, spec 017. Runs the formatters (check
mode), clang-tidy on lib/, ESLint, ruff, the size limits and the structure
and suppression checks, then compares the findings with the committed
baseline (tools/quality/baseline.json). Existing findings are tracked per
rule per file: new ones fail, and so does a fix that isn't recorded, so the
baseline only shrinks (FR-013). Formatting has no baseline (FR-016).

    python3 tools/quality/check.py                    # everything (CI)
    python3 tools/quality/check.py --fast             # skip clang-tidy
    python3 tools/quality/check.py --fix              # format + safe autofixes, then check
    python3 tools/quality/check.py --update-baseline  # record today's findings (justify in the PR)

Run it with the Python that has tools/quality/requirements.txt installed.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from checks import (  # noqa: E402 - after the path is set up
    CPP_PATTERNS,
    HINTS,
    PY_PATTERNS,
    ROOT,
    Finding,
    check_clang_format,
    check_clang_tidy,
    check_eslint,
    check_file_lengths,
    check_lib_purity,
    check_lizard,
    check_prettier,
    check_ruff,
    check_ruff_format,
    check_suppressions,
    python_nesting,
    tracked,
)

BASELINE = ROOT / "tools" / "quality" / "baseline.json"


# --- baseline ------------------------------------------------------------------


def count(findings: list[Finding]) -> dict[str, dict[str, int]]:
    counts: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    for f in findings:
        counts[f.rule][f.path] += 1
    return {rule: dict(sorted(files.items())) for rule, files in sorted(counts.items())}


def compare(current: dict[str, dict[str, int]], baseline: dict[str, dict[str, int]]):
    """(rule, path, now, allowed) for every rule/file over its baseline, and the burned-down ones."""
    over, under = [], []
    for rule in sorted(set(current) | set(baseline)):
        now_files, base_files = current.get(rule, {}), baseline.get(rule, {})
        for path in sorted(set(now_files) | set(base_files)):
            now, allowed = now_files.get(path, 0), base_files.get(path, 0)
            if now > allowed:
                over.append((rule, path, now, allowed))
            elif now < allowed:
                under.append((rule, path, now, allowed))
    return over, under


def load_baseline() -> dict[str, dict[str, int]]:
    if not BASELINE.exists():
        return {}
    return json.loads(BASELINE.read_text(encoding="utf-8"))["counts"]


def save_baseline(counts: dict[str, dict[str, int]]) -> None:
    doc = {
        "about": "Existing coding-standard findings per rule per file (spec 017). New findings fail CI; "
        "burn these down and run `python3 tools/quality/check.py --update-baseline`.",
        "counts": counts,
    }
    BASELINE.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")


# --- main ----------------------------------------------------------------------


def apply_fixes(cpp: list[str]) -> None:
    """Linter autofixes first, then the formatters, so the result is formatted."""
    check_eslint(fix=True)
    check_ruff(fix=True)
    check_clang_format(cpp, fix=True)
    check_prettier(fix=True)
    check_ruff_format(fix=True)


def collect(fast: bool, fix: bool) -> list[Finding]:
    cpp, py = tracked(CPP_PATTERNS), tracked(PY_PATTERNS)
    if fix:
        apply_fixes(cpp)
    findings = check_clang_format(cpp, False) + check_prettier(False) + check_ruff_format(False)
    findings += check_eslint(False) + check_ruff(False)
    if not fast:
        findings += check_clang_tidy(cpp)
    findings += check_lizard(cpp, py) + check_file_lengths(cpp, py) + check_lib_purity(cpp) + check_suppressions(cpp, py)
    for path in py:
        findings += python_nesting(path)
    return findings


def report(findings: list[Finding], over, under) -> None:
    by_key = defaultdict(list)
    for f in findings:
        by_key[(f.rule, f.path)].append(f)
    for rule, path, now, allowed in over:
        extra = f"{now - allowed} new" if allowed else "new"
        print(f"\n{rule} {path}: {now} finding(s), baseline {allowed} ({extra}) - {HINTS.get(rule, '')}")
        for f in sorted(by_key[(rule, path)], key=lambda f: f.line):
            print(f"  {f.path}:{f.line}: {f.message}")
    if under:
        print(f"\nBurned down: {sum(a - n for _, _, n, a in under)} finding(s) fewer than the baseline - well done:")
        for rule, path, now, allowed in under:
            print(f"  {rule} {path}: {now} (baseline {allowed})")
        print("Record it in this change: `python3 tools/quality/check.py --update-baseline`.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fast", action="store_true", help="skip clang-tidy (the slow part)")
    parser.add_argument("--fix", action="store_true", help="format everything and apply safe linter fixes first")
    parser.add_argument("--update-baseline", action="store_true", help="record the current findings as the baseline")
    args = parser.parse_args(argv)

    findings = collect(args.fast, args.fix)
    current = count(findings)
    if args.update_baseline:
        if args.fast:
            sys.exit("check.py: --update-baseline needs the full run (no --fast)")
        if "FMT-01" in current:
            sys.exit("check.py: formatting has no baseline - run with --fix first")
        save_baseline(current)
        print(f"Baseline updated: {len(findings)} finding(s) in {sum(len(v) for v in current.values())} rule/file pair(s).")
        return 0

    baseline = load_baseline()
    if args.fast:  # clang-tidy didn't run: don't count its rules as burned down
        baseline = {
            rule: {p: n for p, n in files.items() if not p.startswith("lib/") or rule not in ("LINT-01", "NAME-01", "NAME-02", "NAME-03")}
            for rule, files in baseline.items()
        }
    baseline.pop("FMT-01", None)  # formatting is never baselined
    over, under = compare(current, baseline)
    report(findings, over, under)
    if over:
        print(f"\nFAILED: {len(over)} rule/file pair(s) over the baseline. See docs/development/coding-standards.md;")
        print("fix them, or justify a one-place exception (EXC-01).")
        return 1
    if under:
        print("\nFAILED: the baseline must shrink with the fix (FR-013).")
        return 1
    print(f"\nOK: {len(findings)} finding(s), none new.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
