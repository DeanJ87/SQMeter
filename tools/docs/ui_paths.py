#!/usr/bin/env python3
"""Docs name the UI the way the UI names itself (DS-27, docs drift).

Every **Settings → Tab → Card → Control** path in the docs must use labels
that exist in the English UI strings (web/src/i18n/en.json), so a renamed
tab, card or control can't leave the docs pointing at something that's gone.

  python3 tools/docs/ui_paths.py          # report; exit 1 on findings
  python3 tools/docs/ui_paths.py --json   # [{file, line, message}] for tools/quality

Standard library only.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGLISH = ROOT / "web/src/i18n/en.json"
DOCS = [ROOT / "README.md", *sorted((ROOT / "docs").rglob("*.md"))]
# A step can't contain "→" itself, so the steps can't overlap (no catastrophic backtracking).
PATH = re.compile(r"\*\*((?:Settings|System|Alpaca|Updates|Dashboard)(?: → [^*→]+)+)\*\*")


COMPONENTS = ROOT / "web/src/components"
# Product names used as titles aren't translated, so they're literals in the
# components ("ASCOM Alpaca", "MQTT", "WiFi"): those count as labels too.
LITERAL_TITLE = re.compile(r'\btitle="([^"{}]+)"')


def labels() -> set[str]:
    messages = json.loads(ENGLISH.read_text(encoding="utf-8"))
    out = set()
    for value in messages.values():
        texts = value.values() if isinstance(value, dict) else [value]
        for text in texts:
            if isinstance(text, str):
                out.add(text.strip().rstrip(".?").lower())
    for source in COMPONENTS.rglob("*.tsx"):
        out.update(m.group(1).strip().lower() for m in LITERAL_TITLE.finditer(source.read_text(encoding="utf-8")))
    return out


def findings() -> list[dict]:
    known = labels()
    out = []
    for doc in DOCS:
        for number, line in enumerate(doc.read_text(encoding="utf-8").splitlines(), 1):
            for match in PATH.finditer(line):
                steps = [s.strip() for s in match.group(1).split("→")]
                unknown = [s for s in steps if s.lower() not in known]
                if unknown:
                    out.append(
                        {
                            "file": str(doc.relative_to(ROOT)),
                            "line": number,
                            "message": f"{match.group(1)}: no UI label {', '.join(repr(u) for u in unknown)} (renamed or removed?)",
                        }
                    )
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    found = findings()
    if args.json:
        print(json.dumps(found))
        return 0
    for f in found:
        print(f"{f['file']}:{f['line']}: {f['message']}")
    print(f"{len(found)} docs path(s) name UI that doesn't exist" if found else "OK: docs name the UI as it is")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
