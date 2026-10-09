#!/usr/bin/env python3
"""Device web UI size budgets (coding standard SIZE-01, SIZE-02).

Measures the LittleFS data folder as packed by tools/ui/pack_data.py and:

- SIZE-02: fails when the files as stored, plus one language file at its
  64 KB limit, plus LittleFS block overhead, take more than 75% of the
  smallest LittleFS partition (the rest is headroom for the filesystem's
  copy-on-write and for growth).
- SIZE-01: fails when the gzipped JS + CSS grows more than 10 KB over the
  recorded baseline (tools/ui/size-baseline.json). A change that genuinely
  needs more records it with --update-baseline, which the pull request then
  has to justify.

Prints a table either way.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

BLOCK_BYTES = 4096  # LittleFS block size on the ESP32
LANGUAGE_FILE_MAX_BYTES = 64 * 1024  # lib/LanguageLogic MAX_FILE_BYTES
LANGUAGE_EXTRA_FILES = 2  # /lang.meta and the download marker, one block each
FILL_LIMIT = 0.75
GROWTH_LIMIT_BYTES = 10 * 1024


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


def evaluate(m: dict, partitions: dict[str, int], baseline: dict | None) -> tuple[list[str], list[tuple[str, str]]]:
    """Returns (failures, table rows)."""
    language_blocks = blocks_for(LANGUAGE_FILE_MAX_BYTES) + LANGUAGE_EXTRA_FILES
    used = (m["ui_blocks"] + language_blocks) * BLOCK_BYTES
    smallest_name, smallest = min(partitions.items(), key=lambda kv: kv[1])
    limit = int(smallest * FILL_LIMIT)
    js_css = m["js"] + m["css"]
    rows = [
        ("JS (gzipped)", f"{m['js'] / 1024:.1f} KB"),
        ("CSS (gzipped)", f"{m['css'] / 1024:.1f} KB"),
        ("JS + CSS (gzipped)", f"{js_css / 1024:.1f} KB"),
        ("Fonts", f"{m['fonts'] / 1024:.1f} KB"),
        ("UI files as stored", f"{m['stored'] / 1024:.1f} KB in {len(m['files'])} files"),
        ("UI on LittleFS (blocks)", f"{m['ui_blocks'] * BLOCK_BYTES / 1024:.0f} KB"),
        ("Language file (at its limit)", f"{language_blocks * BLOCK_BYTES / 1024:.0f} KB"),
        ("Total on LittleFS", f"{used / 1024:.0f} KB"),
        (f"Partition ({smallest_name})", f"{smallest / 1024:.0f} KB"),
        ("Used / limit", f"{used / smallest:.0%} / {FILL_LIMIT:.0%}"),
    ]
    failures = []
    if used > limit:
        failures.append(
            f"SIZE-02: the UI plus a language file need {used / 1024:.0f} KB of LittleFS, "
            f"over {FILL_LIMIT:.0%} of the {smallest / 1024:.0f} KB partition ({limit / 1024:.0f} KB)."
        )
    if baseline is not None:
        growth = js_css - baseline["jsCssGzipBytes"]
        rows.append(("Growth over baseline", f"{growth / 1024:+.1f} KB (limit +{GROWTH_LIMIT_BYTES / 1024:.0f} KB)"))
        if growth > GROWTH_LIMIT_BYTES:
            failures.append(
                f"SIZE-01: gzipped JS + CSS grew {growth / 1024:.1f} KB over the baseline "
                f"(limit {GROWTH_LIMIT_BYTES / 1024:.0f} KB). Trim it, or if the feature needs it, "
                "run tools/ui/size_check.py --update-baseline and justify it in the pull request."
            )
    return failures, rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Device web UI size budgets (SIZE-01, SIZE-02)")
    parser.add_argument("--data", type=Path, default=Path("data"))
    parser.add_argument("--partitions", type=Path, nargs="+", default=[Path("partitions.csv"), Path("partitions_ble.csv")])
    parser.add_argument("--baseline", type=Path, default=Path(__file__).with_name("size-baseline.json"))
    parser.add_argument("--update-baseline", action="store_true")
    args = parser.parse_args(argv)

    if not (args.data / "index.html.gz").exists():
        print(f"{args.data} is not a packed UI (run tools/ui/pack_data.py first)", file=sys.stderr)
        return 2
    m = measure(args.data)
    if args.update_baseline:
        args.baseline.write_text(json.dumps({"jsCssGzipBytes": m["js"] + m["css"]}, indent=2) + "\n")
        print(f"Baseline recorded: {(m['js'] + m['css']) / 1024:.1f} KB gzipped JS + CSS")
    baseline = json.loads(args.baseline.read_text()) if args.baseline.exists() else None
    partitions = {p.name: partition_bytes(p) for p in args.partitions}
    failures, rows = evaluate(m, partitions, baseline)

    width = max(len(k) for k, _ in rows)
    print("Device web UI size")
    for key, value in rows:
        print(f"  {key:<{width}}  {value}")
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
