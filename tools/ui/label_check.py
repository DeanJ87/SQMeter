#!/usr/bin/env python3
"""One name per thing across the UI, Home Assistant and the docs (specs/026 FR-013, DS-27).

Reads tools/i18n/glossary/en.json and fails when:
  - an en.json string listed for a name doesn't say that name (case aside);
  - another name for the same thing (`avoid`, as written) appears in UI text
    (en.json, device text excepted) or in the docs (hardware pages excepted);
  - the Home Assistant entity name in lib/Readings doesn't match.

  python3 tools/ui/label_check.py          # report; exit 1 on findings
  python3 tools/ui/label_check.py --json   # [{file, line, message}] for tools/quality

Standard library only.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GLOSSARY = "tools/i18n/glossary/en.json"
ENGLISH = "web/src/i18n/en.json"
READINGS = "lib/Readings/src/Readings.cpp"
DOCS = "docs"


def strings(english: dict) -> dict:
    """Key -> text for UI strings (plural forms joined), device text left out."""
    out = {}
    for key, value in english.items():
        if key.startswith("device."):
            continue
        out[key] = " ".join(value.values()) if isinstance(value, dict) else str(value)
    return out


def check_keys(entry: dict, english: dict) -> list[tuple[str, str]]:
    problems = []
    for key in entry["keys"]:
        text = english.get(key)
        if text is None:
            problems.append((ENGLISH, f"{key} is listed for '{entry['name']}' but isn't in en.json"))
        elif str(text).strip().lower() != entry["name"].lower():
            problems.append((ENGLISH, f"{key} says {text!r}: the name is '{entry['name']}' (DS-27)"))
    return problems


def check_avoid(entry: dict, ui: dict, docs: dict) -> list[tuple[str, str]]:
    problems = []
    for variant in entry.get("avoid", []):
        # As written: "the TSL2591 light sensor" in prose is the name with its part number, not another name.
        pattern = re.compile(rf"(?<![\w-]){re.escape(variant)}(?![\w-])")
        problems += [(ENGLISH, f"{key} calls '{entry['name']}' {variant!r}") for key, text in ui.items() if pattern.search(text)]
        problems += [(path, f"the docs call '{entry['name']}' {variant!r}") for path, text in docs.items() if pattern.search(text)]
    return problems


def check(root: Path = ROOT) -> list[tuple[str, str]]:
    glossary = json.loads((root / GLOSSARY).read_text(encoding="utf-8"))
    english = json.loads((root / ENGLISH).read_text(encoding="utf-8"))
    ui = strings(english)
    # Hardware pages (wiring, parts lists) name the parts themselves.
    pages = [p for p in sorted((root / DOCS).rglob("*.md")) if "hardware" not in p.relative_to(root / DOCS).parts]
    docs = {str(p.relative_to(root)): p.read_text(encoding="utf-8") for p in pages}
    readings = (root / READINGS).read_text(encoding="utf-8")
    problems = []
    for entry in glossary["names"]:
        problems += check_keys(entry, english)
        problems += check_avoid(entry, ui, docs)
        if entry.get("ha") and f'"{entry["ha"]}"' not in readings:
            problems.append((READINGS, f"no Home Assistant entity named '{entry['ha']}' for '{entry['name']}'"))
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    problems = check()
    if args.json:
        print(json.dumps([{"file": path, "line": 1, "message": message} for path, message in problems]))
        return 0
    for path, message in problems:
        print(f"DS-LABEL: {path}: {message}")
    print(f"{len(problems)} problem(s)" if problems else "OK: one name per thing (DS-27)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
