#!/usr/bin/env python3
"""Firmware flash budget (coding standard SIZE-04, spec 027 FR-001..FR-003).

For each firmware build (esp32dev, esp32dev-ble), compares firmware.bin with
the app slot in partitions.csv:

- warns (GitHub annotation) above 90% of the slot, fails above 95%, listing
  the largest contributors from the linker map (tools/firmware/size_map.py);
- reports the change from the branch it merges into. Sizes are recorded in
  tools/firmware/size-baseline.json; each change keeps that record current
  (within 2 KB, --update-baseline) and is measured against the record on the
  target branch (--base-ref).

A build that genuinely needs more than 95% records why with
--update-baseline --reason "..." (justified in the pull request), as SIZE-01
does for the web UI.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def _load(name: str):
    """Imports a sibling tool by path (the tools folder isn't a package)."""
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(f"{name}.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


size_map = _load("size_map")

WARN_LEVEL = 0.90
FAIL_LEVEL = 0.95
STALE_TOLERANCE_BYTES = 2 * 1024
BASELINE_PATH = "tools/firmware/size-baseline.json"
REASON_KEY = "overBudgetReason"
ENVS = ("esp32dev", "esp32dev-ble")
TOP = 10


def app_slot_bytes(csv: Path) -> int:
    """Size of the first OTA app slot (ota_0) in a partition table."""
    for line in csv.read_text().splitlines():
        if line.lstrip().startswith("#"):
            continue
        fields = [f.strip() for f in line.split(",")]
        if len(fields) >= 5 and fields[1] == "app" and fields[2] == "ota_0":
            return int(fields[4], 0)
    raise ValueError(f"no ota_0 app partition in {csv}")


def kb(size: float) -> str:
    return f"{size / 1024:.1f} KB"


def evaluate(env: str, size: int, slot: int, record: dict, base: dict | None) -> tuple[list[str], list[str], list[str]]:
    """Returns (failures, warnings, report lines) for one build."""
    failures: list[str] = []
    warnings: list[str] = []
    used = size / slot
    lines = [f"{env}: {size:,} B of {slot:,} B ({used:.1%}), {kb(slot - size)} free"]
    recorded = record.get(env)
    if recorded is not None and abs(size - recorded) > STALE_TOLERANCE_BYTES:
        failures.append(
            f"SIZE-04: {BASELINE_PATH} records {recorded:,} B for {env} but the build is {size:,} B. "
            "Record it: python3 tools/firmware/flash_budget.py --update-baseline"
        )
    reference = (base or {}).get(env, recorded)
    if reference is not None:
        lines.append(f"  change from {'the target branch' if base else 'the record'}: {size - reference:+,} B")
    reason = record.get(REASON_KEY, "")
    waived = bool(reason) and reason != (base or {}).get(REASON_KEY, "")
    if used > FAIL_LEVEL and waived:
        lines.append(f"  over {FAIL_LEVEL:.0%}, recorded reason: {reason}")
    elif used > FAIL_LEVEL:
        failures.append(
            f"SIZE-04: {env} uses {used:.1%} of its app slot (limit {FAIL_LEVEL:.0%}). Trim it (largest contributors below), "
            'or if the feature needs it, run tools/firmware/flash_budget.py --update-baseline --reason "..." '
            "and justify it in the pull request."
        )
    elif used > WARN_LEVEL:
        warnings.append(f"SIZE-04: {env} uses {used:.1%} of its app slot (warning above {WARN_LEVEL:.0%}).")
    return failures, warnings, lines


def contributors(map_file: Path) -> list[str]:
    if not map_file.exists():
        return [f"  (no linker map at {map_file})"]
    flash = size_map.totals(size_map.parse(map_file), size_map.FLASH)
    return [f"  {name:44} {size:>9,} B" for name, size in sorted(flash.items(), key=lambda kv: -kv[1])[:TOP]]


def record_at(ref: str) -> dict | None:
    result = subprocess.run(["git", "show", f"{ref}:{BASELINE_PATH}"], cwd=ROOT, capture_output=True, text=True, check=False)
    return json.loads(result.stdout) if result.returncode == 0 else None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Firmware flash budget (SIZE-04)")
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".pio" / "build")
    parser.add_argument("--partitions", type=Path, default=ROOT / "partitions.csv")
    parser.add_argument("--baseline", type=Path, default=ROOT / BASELINE_PATH)
    parser.add_argument("--base-ref", help="git ref of the branch this change merges into (e.g. origin/main)")
    parser.add_argument("--update-baseline", action="store_true", help="record the current sizes")
    parser.add_argument("--reason", default="", help="with --update-baseline: why a build needs more than 95%%")
    parser.add_argument("--envs", nargs="+", default=list(ENVS))
    args = parser.parse_args(argv)

    slot = app_slot_bytes(args.partitions)
    sizes = {}
    for env in args.envs:
        image = args.build_dir / env / "firmware.bin"
        if not image.exists():
            print(f"{image} not found (build {env} first)", file=sys.stderr)
            return 2
        sizes[env] = image.stat().st_size

    if args.update_baseline:
        record = dict(sizes)
        if args.reason:
            record[REASON_KEY] = args.reason
        args.baseline.write_text(json.dumps(record, indent=2) + "\n")
        print(f"Recorded {', '.join(f'{e} {s:,} B' for e, s in sizes.items())} in {args.baseline}")
    record = json.loads(args.baseline.read_text()) if args.baseline.exists() else {}
    base = record_at(args.base_ref) if args.base_ref else None

    failures: list[str] = []
    for env, size in sizes.items():
        env_failures, env_warnings, lines = evaluate(env, size, slot, record, base)
        print("\n".join(lines))
        if env_failures or env_warnings:
            print(f"  largest contributors ({env}):")
            print("\n".join(contributors(args.build_dir / env / "firmware.map")))
        for warning in env_warnings:
            print(f"::warning::{warning}")
        failures += env_failures
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
