#!/usr/bin/env python3
"""Device web UI size budgets (coding standard SIZE-01, SIZE-02).

Measures the LittleFS data folder as packed by tools/ui/pack_data.py and:

- SIZE-02: fails when the files as stored, plus one language file at its
  64 KB limit, plus LittleFS block overhead, take more than 75% of the
  smallest LittleFS partition (the rest is headroom for the filesystem's
  copy-on-write and for growth).
- SIZE-01: fails when the gzipped JS + CSS grows more than 10 KB in one
  change. The size is recorded in tools/ui/size-baseline.json; each change
  keeps that record current (within 2 KB, --update-baseline) and is measured
  against the record on the branch it merges into (--base-ref). A change that
  genuinely needs more records why with --update-baseline --reason "...".

Prints a table either way.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path

BLOCK_BYTES = 4096  # LittleFS block size on the ESP32
LANGUAGE_FILE_MAX_BYTES = 64 * 1024  # lib/LanguageLogic MAX_FILE_BYTES
LANGUAGE_EXTRA_FILES = 2  # /lang.meta and the download marker, one block each
FILL_LIMIT = 0.75
GROWTH_LIMIT_BYTES = 10 * 1024
STALE_TOLERANCE_BYTES = 2 * 1024
BASELINE_PATH = "tools/ui/size-baseline.json"
REASON_KEY = "overBudgetReason"


def blocks_for(size: int) -> int:
    """Blocks a file of `size` bytes occupies: its data plus one metadata block."""
    return max(1, math.ceil(size / BLOCK_BYTES)) + 1


def partition_bytes(csv: Path) -> int:
    """Size of the LittleFS ("spiffs" subtype) partition in a partition table."""
    for line in csv.read_text().splitlines():
        fields = [f.strip() for f in line.split(",")]
        if len(fields) >= 5 and not line.lstrip().startswith("#") and fields[2] == "spiffs":
            return int(fields[4], 0)
    raise ValueError(f"no LittleFS partition in {csv}")


def is_kind(name: str, ext: str) -> bool:
    """`name` is a `.ext` file, stored plain or gzipped."""
    return re.search(rf"\.{ext}(\.gz)?$", name) is not None


def measure(data: Path) -> dict:
    files = sorted(p for p in data.rglob("*") if p.is_file())
    sizes = {p.relative_to(data).as_posix(): p.stat().st_size for p in files}
    return {
        "files": sizes,
        "js": sum(s for n, s in sizes.items() if is_kind(n, "js")),
        "css": sum(s for n, s in sizes.items() if is_kind(n, "css")),
        "fonts": sum(s for n, s in sizes.items() if n.endswith(".woff2")),
        "stored": sum(sizes.values()),
        "ui_blocks": sum(blocks_for(s) for s in sizes.values()),
    }


def kb(size: float) -> str:
    return f"{size / 1024:.1f} KB"


def fill_check(m: dict, partitions: dict[str, int]) -> tuple[list[str], list[tuple[str, str]]]:
    """SIZE-02: the UI plus a language file at its limit fit 75% of the smallest partition."""
    language_blocks = blocks_for(LANGUAGE_FILE_MAX_BYTES) + LANGUAGE_EXTRA_FILES
    used = (m["ui_blocks"] + language_blocks) * BLOCK_BYTES
    smallest_name, smallest = min(partitions.items(), key=lambda kv: kv[1])
    limit = int(smallest * FILL_LIMIT)
    rows = [
        ("UI on LittleFS (with block overhead)", kb(m["ui_blocks"] * BLOCK_BYTES)),
        ("Language file at its 64 KB limit", kb(language_blocks * BLOCK_BYTES)),
        ("Total on LittleFS", kb(used)),
    ]
    rows += [(f"LittleFS partition ({name})", kb(size)) for name, size in sorted(partitions.items())]
    rows.append(("Used / limit", f"{used / smallest:.0%} of {smallest_name} / {FILL_LIMIT:.0%} ({kb(limit)})"))
    failures = []
    if used > limit:
        failures.append(
            f"SIZE-02: the UI plus a language file need {kb(used)} of LittleFS, "
            f"over {FILL_LIMIT:.0%} of the {kb(smallest)} partition ({kb(limit)})."
        )
    return failures, rows


def growth_check(js_css: int, baseline: dict | None, base: dict | None) -> tuple[list[str], list[tuple[str, str]]]:
    """SIZE-01: the record is current, and this change grew JS + CSS by at most 10 KB over `base`."""
    failures: list[str] = []
    rows: list[tuple[str, str]] = []
    if baseline is not None:
        recorded = baseline["jsCssGzipBytes"]
        rows.append(("Recorded in size-baseline.json", kb(recorded)))
        if abs(js_css - recorded) > STALE_TOLERANCE_BYTES:
            failures.append(
                f"SIZE-01: {BASELINE_PATH} records {kb(recorded)} gzipped JS + CSS but the UI is {kb(js_css)}. "
                "Record it: python3 tools/ui/size_check.py --update-baseline"
            )
    reference = base if base is not None else baseline
    if reference is None:
        return failures, rows
    growth = js_css - reference["jsCssGzipBytes"]
    where = "the target branch" if base is not None else "the record"
    rows.append((f"Growth over {where}", f"{growth / 1024:+.1f} KB (limit +{GROWTH_LIMIT_BYTES / 1024:.0f} KB)"))
    reason = (baseline or {}).get(REASON_KEY, "")
    waived = bool(reason) and reason != reference.get(REASON_KEY, "")
    if growth > GROWTH_LIMIT_BYTES and waived:
        rows.append(("Over budget, recorded reason", reason))
    elif growth > GROWTH_LIMIT_BYTES:
        failures.append(
            f"SIZE-01: gzipped JS + CSS grew {growth / 1024:.1f} KB in this change (limit {GROWTH_LIMIT_BYTES / 1024:.0f} KB). "
            'Trim it, or if the feature needs it, run tools/ui/size_check.py --update-baseline --reason "..." '
            "and justify it in the pull request."
        )
    return failures, rows


def evaluate(
    m: dict, partitions: dict[str, int], baseline: dict | None, base: dict | None = None
) -> tuple[list[str], list[tuple[str, str]]]:
    """Returns (failures, summary table rows)."""
    js_css = m["js"] + m["css"]
    rows = [
        ("JS (gzipped)", kb(m["js"])),
        ("CSS (gzipped)", kb(m["css"])),
        ("JS + CSS (gzipped)", kb(js_css)),
        ("Fonts (woff2)", kb(m["fonts"])),
        ("UI files as stored", f"{kb(m['stored'])} in {len(m['files'])} files"),
    ]
    fill_failures, fill_rows = fill_check(m, partitions)
    growth_failures, growth_rows = growth_check(js_css, baseline, base)
    return fill_failures + growth_failures, rows + fill_rows + growth_rows


def baseline_at(ref: str, root: Path) -> dict | None:
    """The size record on git `ref`, or None when that branch has none yet."""
    result = subprocess.run(["git", "show", f"{ref}:{BASELINE_PATH}"], cwd=root, capture_output=True, text=True, check=False)
    return json.loads(result.stdout) if result.returncode == 0 else None


def print_report(m: dict, rows: list[tuple[str, str]], failures: list[str]) -> None:
    print("Device web UI (LittleFS data folder)")
    width = max(len(n) for n in m["files"])
    for name, size in m["files"].items():
        print(f"  {name:<{width}}  {size / 1024:7.1f} KB  {blocks_for(size) * BLOCK_BYTES / 1024:5.0f} KB on flash")
    print()
    width = max(len(k) for k, _ in rows)
    for key, value in rows:
        print(f"  {key:<{width}}  {value}")
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Device web UI size budgets (SIZE-01, SIZE-02)")
    parser.add_argument("--data", type=Path, default=Path("data"))
    parser.add_argument("--partitions", type=Path, nargs="+", default=[Path("partitions.csv"), Path("partitions_ble.csv")])
    parser.add_argument("--baseline", type=Path, default=Path(__file__).with_name("size-baseline.json"))
    parser.add_argument("--base-ref", help="git ref of the branch this change merges into (e.g. origin/main)")
    parser.add_argument("--update-baseline", action="store_true", help="record the current size")
    parser.add_argument("--reason", default="", help="with --update-baseline: why this change needs more than 10 KB")
    args = parser.parse_args(argv)

    if not (args.data / "index.html.gz").exists():
        print(f"{args.data} is not a packed UI (run tools/ui/pack_data.py first)", file=sys.stderr)
        return 2
    m = measure(args.data)
    if args.update_baseline:
        record = {"jsCssGzipBytes": m["js"] + m["css"]}
        if args.reason:
            record[REASON_KEY] = args.reason
        args.baseline.write_text(json.dumps(record, indent=2) + "\n")
        print(f"Recorded {kb(record['jsCssGzipBytes'])} gzipped JS + CSS in {args.baseline}")
    baseline = json.loads(args.baseline.read_text()) if args.baseline.exists() else None
    base = baseline_at(args.base_ref, Path(__file__).resolve().parents[2]) if args.base_ref else None
    partitions = {p.name: partition_bytes(p) for p in args.partitions}
    failures, rows = evaluate(m, partitions, baseline, base)
    print_report(m, rows, failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
