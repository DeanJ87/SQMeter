#!/usr/bin/env python3
"""The dashboard inventory covers every piece of device state (specs/025 FR-002, FR-003).

Every property path of the status, readings, safety and alerts-armed contract
schemas, and every settings-dependency id, must be mapped by web/src/dashboard/
inventory.json: shown (with a test naming it in web/tests/dashboard.spec.ts) or
not shown (with a reason). Fails on an unmapped path or id, a stale mapping, a
shown entry without a test or a label key missing from en.json.

  python3 tools/dashboard/check.py          # check (quality gate DASH-02, build)
  python3 tools/dashboard/check.py --json   # [{file, line, message}] for tools/quality
  python3 tools/dashboard/check.py --list   # every path and what maps it

Standard library only.
"""

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMAS = "specs/016-demo-device-emulation/contracts/schemas"
SCHEMA_NAMES = ["status", "readings", "safety", "alerts-armed"]
CATALOGUE = "lib/SettingsDeps/catalogue.json"
INVENTORY = "web/src/dashboard/inventory.json"
TESTS = "web/tests/dashboard.spec.ts"
# A state the demo can't produce is tested on the component instead, with the reason recorded.
UNIT_TESTS = "web/src/dashboard/__tests__"
ENGLISH = "web/src/i18n/en.json"


def read_json(root: Path, rel: str):
    return json.loads((root / rel).read_text(encoding="utf-8"))


def schema_paths(node: dict, prefix: str, defs: dict, out: set) -> None:
    """Every property path under node: `a.b`, arrays as `a[]`, maps as `a.*`."""
    if "$ref" in node:
        node = defs.get(node["$ref"].split("/")[-1], {})
    for name, child in node.get("properties", {}).items():
        path = f"{prefix}.{name}"
        out.add(path)
        schema_paths(child, path, defs, out)
    if node.get("type") == "array" and isinstance(node.get("items"), dict):
        schema_paths(node["items"], prefix + "[]", defs, out)
    if isinstance(node.get("additionalProperties"), dict):
        schema_paths(node["additionalProperties"], prefix + ".*", defs, out)
    for key in ("oneOf", "anyOf", "allOf"):
        for sub in node.get(key, []):
            schema_paths(sub, prefix, defs, out)


def device_state(root: Path) -> set:
    """Schema paths (`<schema>.<path>`) and dependency ids (`deps.D-NN`)."""
    out = set()
    for name in SCHEMA_NAMES:
        doc = read_json(root, f"{SCHEMAS}/{name}.schema.json")
        paths = set()
        schema_paths(doc, name, doc.get("$defs", doc.get("definitions", {})), paths)
        out |= paths
    out |= {f"deps.{e['id']}" for e in read_json(root, CATALOGUE)["entries"]}
    return out


def matches(pattern: str, path: str) -> bool:
    """`x.**` matches x and everything under it; otherwise an exact path."""
    if pattern.endswith(".**"):
        base = pattern[:-3]
        return path == base or path.startswith(base + ".") or path.startswith(base + "[]")
    return path == pattern


def mappings(inventory: dict) -> list:
    """(pattern, owner) for every shown source and not-shown match."""
    out = [(source, f"shown:{entry['id']}") for entry in inventory["shown"] for source in entry["sources"]]
    return out + [(item["match"], "notShown") for item in inventory["notShown"]]


def check_coverage(state: set, inventory: dict) -> list:
    problems = []
    patterns = mappings(inventory)
    for path in sorted(state):
        if not any(matches(pattern, path) for pattern, _ in patterns):
            problems.append(
                f"{path} isn't on the dashboard inventory: add it to a shown entry's sources, or to notShown with a reason ({INVENTORY})"
            )
    for pattern, owner in patterns:
        if not any(matches(pattern, path) for path in state):
            problems.append(f"{pattern} ({owner}) matches no field or dependency id any more: remove or rename it")
    return problems


def check_entries(inventory: dict, tests: str, english: dict, unit_tests: str = "") -> list:
    problems, seen = [], set()
    for entry in inventory["shown"]:
        if entry["id"] in seen:
            problems.append(f"shown entry {entry['id']} is listed twice")
        seen.add(entry["id"])
        unit = entry.get("testIn") == "unit"
        if unit and not entry.get("demoCannot"):
            problems.append(f"shown entry {entry['id']} is tested outside the demo: say why in `demoCannot`")
        if f"inventory: {entry['test']}" not in (unit_tests if unit else tests):
            where = UNIT_TESTS if unit else TESTS
            problems.append(f"shown entry {entry['id']} has no test: add `inventory: {entry['test']}` to a test in {where} (FR-003)")
        if entry["label"] not in english:
            problems.append(f"shown entry {entry['id']}: label {entry['label']} isn't in {ENGLISH}")
        if entry["visibility"]["rule"] == "when" and not entry["visibility"].get("when"):
            problems.append(f"shown entry {entry['id']}: a `when` rule needs its condition")
    problems += [f"notShown {item['match']} needs a reason" for item in inventory["notShown"] if not item.get("reason")]
    return problems


def check(root: Path = ROOT) -> list:
    inventory = read_json(root, INVENTORY)
    tests_path = root / TESTS
    tests = tests_path.read_text(encoding="utf-8") if tests_path.exists() else ""
    unit_dir = root / UNIT_TESTS
    unit = "\n".join(p.read_text(encoding="utf-8") for p in sorted(unit_dir.glob("*.test.ts*"))) if unit_dir.exists() else ""
    return check_coverage(device_state(root), inventory) + check_entries(inventory, tests, read_json(root, ENGLISH), unit)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    if args.list:
        patterns = mappings(read_json(ROOT, INVENTORY))
        for path in sorted(device_state(ROOT)):
            owners = sorted({owner for pattern, owner in patterns if matches(pattern, path)})
            print(f"{path}: {', '.join(owners) or 'UNMAPPED'}")
        return 0
    problems = check()
    if args.json:
        print(json.dumps([{"file": INVENTORY, "line": 1, "message": p} for p in problems]))
        return 0
    for problem in problems:
        print(f"DASH-02: {problem}")
    print(f"{len(problems)} problem(s)" if problems else "OK: every device field and dependency is on the dashboard inventory")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
