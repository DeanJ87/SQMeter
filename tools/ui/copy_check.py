#!/usr/bin/env python3
"""English copy rules for the web UI (specs/026 FR-012, DS-21..DS-25).

Reads web/src/i18n/en.json with each key's context note (en.context.json) and
fails on:
  - a string longer than its type allows: badge/pill 16, label/title/button 32,
    note/sentence 90, "?" hint 160 characters (English; translations follow);
  - a banned filler phrase (DS-25);
  - a run-on status line joined with " · ", or a label joined with " - " (DS-24).
The type comes from the context note's wording. Strings that can't follow a
rule are listed in tools/ui/copy-exceptions.json with a reason (EXC-01).

  python3 tools/ui/copy_check.py           # report; exit 1 on findings
  python3 tools/ui/copy_check.py --json    # [{file, line, message}] for tools/quality
  python3 tools/ui/copy_check.py --list    # every checked string with its type and length

Standard library only.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGLISH = "web/src/i18n/en.json"
CONTEXT = "web/src/i18n/en.context.json"
EXCEPTIONS = "tools/ui/copy-exceptions.json"

LIMITS = {"pill": 16, "label": 32, "note": 90, "hint": 160}
# Read in this order: the first match wins.
TYPES = [
    ("hint", re.compile(r"'\?' tip|\? tip|help text|tooltip", re.I)),
    ("pill", re.compile(r"\bbadge\b|\bpill\b", re.I)),
    ("note", re.compile(r"\bsentence\b", re.I)),
    ("label", re.compile(r"\blabel\b|\bheading\b|\btitle\b|\bbutton\b|\btab name\b|\bcolumn header\b", re.I)),
    ("note", re.compile(r"\bnote\b|\bsentence\b|\bmessage\b|\bstatus line\b|\bwarning\b|\berror\b", re.I)),
]
# Not UI copy written here: the device's own text, alert wording sent to
# phones, and screen-reader only text are checked elsewhere or exempt.
SKIP_PREFIXES = ("device.", "a11y.", "alertTemplates.", "demo.")

BANNED = [
    "at a glance",
    "simply",
    "please note",
    "note that",
    "it is important",
    "in order to",
    "ensure",
    "seamless",
    "worked out in",
    "as soon as the device has it",
]
RUN_ON = re.compile(r"\S · \S")
LABEL_JOIN = re.compile(r"\S [-–] \S")  # "Silent for - safety monitor": two facts in one label


def flatten(node: dict, prefix: str = "") -> dict:
    """Key -> text; plural objects check each form."""
    out = {}
    for key, value in node.items():
        path = f"{prefix}{key}"
        if isinstance(value, dict) and not set(value) <= {"zero", "one", "two", "few", "many", "other"}:
            out.update(flatten(value, path + "."))
        elif isinstance(value, dict):
            for form, text in value.items():
                out[f"{path}#{form}"] = text
        else:
            out[path] = value
    return out


def string_type(note: str) -> str | None:
    for name, pattern in TYPES:
        if pattern.search(note or ""):
            return name
    return None


def visible_length(text: str) -> int:
    """Length as read: placeholders count as a short value."""
    return len(re.sub(r"\{[a-zA-Z0-9_]+\}", "xxxx", text))


def check(root: Path = ROOT) -> list[str]:
    english = flatten(json.loads((root / ENGLISH).read_text(encoding="utf-8")))
    context = json.loads((root / CONTEXT).read_text(encoding="utf-8"))
    exceptions_path = root / EXCEPTIONS
    exceptions = json.loads(exceptions_path.read_text(encoding="utf-8")) if exceptions_path.exists() else {}
    problems = []
    for key, text in sorted(english.items()):
        base = key.split("#")[0]
        if base.startswith(SKIP_PREFIXES) or not isinstance(text, str):
            continue
        reason = exceptions.get(base)
        if reason is not None:
            if not str(reason).strip():
                problems.append(f"{base}: an exception needs a reason (EXC-01)")
            continue
        kind = string_type(context.get(base, ""))
        limit = LIMITS.get(kind or "")
        if limit and visible_length(text) > limit:
            problems.append(f"{base}: {kind} is {visible_length(text)} characters, the limit is {limit} (DS-2x): {text!r}")
        lowered = text.lower()
        for phrase in BANNED:
            if re.search(rf"\b{re.escape(phrase)}\b", lowered):
                problems.append(f"{base}: banned phrase {phrase!r} (DS-25)")
        if RUN_ON.search(text):
            problems.append(f"{base}: run-on line joined with ' · ' (DS-24): one fact per tile or row")
        if kind == "label" and LABEL_JOIN.search(text):
            problems.append(f"{base}: label joined with ' - ' (DS-24): one name per label, e.g. 'Safety monitor silent for'")
    problems += [f"{key}: exception for a key that no longer exists" for key in exceptions if key not in {k.split("#")[0] for k in english}]
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    if args.list:
        english = flatten(json.loads((ROOT / ENGLISH).read_text(encoding="utf-8")))
        context = json.loads((ROOT / CONTEXT).read_text(encoding="utf-8"))
        for key, text in sorted(english.items()):
            print(f"{string_type(context.get(key.split('#')[0], '')) or '-':5} {visible_length(str(text)):4} {key}")
        return 0
    problems = check()
    if args.json:
        print(json.dumps([{"file": ENGLISH, "line": 1, "message": p} for p in problems]))
        return 0
    for problem in problems:
        print(f"DS-COPY: {problem}")
    print(f"{len(problems)} problem(s)" if problems else "OK: English copy follows DS-21..DS-25")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
