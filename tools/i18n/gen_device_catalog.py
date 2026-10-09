#!/usr/bin/env python3
"""Device message catalogue (specs/023-i18n FR-008, research D4).

Finds the user-facing text the firmware builds (settings errors, safety reasons,
alert texts, API errors), turns each into a template with {named} placeholders,
and keeps the `device.*` keys of web/src/i18n/en.json in step. The UI recognises
device text against these templates (web/src/i18n/deviceMessage.ts) - the key is
the message's stable ID - and shows the translation (research.md D4).

  python3 tools/i18n/gen_device_catalog.py          update en.json
  python3 tools/i18n/gen_device_catalog.py --check  fail if it is out of date
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EN = ROOT / "web/src/i18n/en.json"
CONTEXT = ROOT / "web/src/i18n/en.context.json"

# file -> (area, calls whose argument N (or "fmt" args) is device text)
SOURCES = {
    "lib/ConfigModel/src/ConfigModel.cpp": ("settings", {"setError": [1]}),
    "lib/AlpacaLogic/src/SafetyEvaluator.cpp": ("safety", {"addReason": [2], "addReasonf": "fmt2"}),
    "lib/AlertLogic/src/AlertEngine.cpp": ("alert", {"make": [1, 2], "format": "fmt0"}),
    "src/WebServer.cpp": ("api", {"createErrorJson": [0]}),
    "src/OtaUpdater.cpp": ("ota", {"errorCb": [0]}),
    "tools/demo-core/bridge.cpp": ("api", {"errorJson": [0]}),
    "lib/LanguageLogic/src/LanguageLogic.cpp": ("language", {}),
    "src/LanguagePack.cpp": ("language", {"fail": [0], "sendError": [2]}),
}
# `<target>["error"] = <expr>;` and `error = <expr>;` assignments are device text too.
ASSIGN = {
    "src/WebServer.cpp": re.compile(r'\b\w+\["error"\]\s*=\s*'),
    "src/OtaUpdater.cpp": re.compile(r"(?<![\w.>])error\s*=\s*(?!=)"),
    "tools/demo-core/bridge.cpp": re.compile(r'\b\w+\["error"\]\s*=\s*'),
    "src/LanguagePack.cpp": re.compile(r"(?<![\w.>])error\s*=\s*(?=\")"),
    "lib/LanguageLogic/src/LanguageLogic.cpp": re.compile(r"(?<![\w.>])(?:error\s*=|return)\s*(?=\")"),
}
PRINTF = re.compile(r"%(?:[-+ 0#]*\d*(?:\.\d+)?)(?:l|ll|h|z)?[dfisuxXgc]|%%")


def scan(text: str, start: int = 0):
    """(index, char, depth) for each character outside string literals."""
    depth, i, quote = 0, start, None
    while i < len(text):
        c = text[i]
        if quote:
            i += 2 if c == "\\" else 1
            quote = None if c == quote else quote
            continue
        if c in "\"'":
            quote = c
        depth += (c in "([{") - (c in ")]}")
        yield i, c, depth
        i += 1


def balanced(src: str, start: int) -> tuple[str, int]:
    """The text inside the parentheses opening at src[start] == '(' and the index after ')'."""
    for i, c, depth in scan(src, start):
        if c in ")]}" and depth == 0:
            return src[start + 1 : i], i + 1
    return src[start + 1 :], len(src)


def split_top(text: str, sep: str) -> list[str]:
    cuts = [i for i, c, depth in scan(text) if c == sep and depth == 0]
    bounds = zip([-1, *cuts], [*cuts, len(text)])
    return [text[a + 1 : b].strip() for a, b in bounds if text[a + 1 : b].strip()]


LITERAL = re.compile(r'^(?:std::string\()?\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)?$')


def literal(expr: str) -> str | None:
    m = LITERAL.match(expr.strip())
    if not m:
        return None
    pieces = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
    return "".join(bytes(p, "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8") for p in pieces)


def param_name(expr: str, used: set[str]) -> str:
    e = expr.strip()
    e = re.sub(r"^std::to_string\((.*)\)$", r"\1", e)
    e = re.sub(r"\.c_str\(\)$", "", e)
    e = re.sub(r"^static_cast<[^>]+>\((.*)\)$", r"\1", e)
    if re.search(r"[-+*/]\s*[A-Za-z_(]", e):  # an expression, e.g. a - b: no single name fits
        e = "value"
    names = re.findall(r"[A-Za-z_]\w*", e)
    name = names[-1] if names else "value"
    name = re.sub(r"^(get|to)(?=[A-Z])", "", name)
    name = name[0].lower() + name[1:] if name else "value"
    base, n = name, 2
    while name in used:
        name, n = f"{base}{n}", n + 1
    used.add(name)
    return name


def from_printf(fmt: str, args: list[str]) -> str | None:
    used: set[str] = set()
    names = iter(args)
    out = []
    pos = 0
    for m in PRINTF.finditer(fmt):
        out.append(fmt[pos : m.start()])
        if m.group(0) == "%%":
            out.append("%")
        else:
            arg = next(names, None)
            if arg is None:
                return None
            out.append("{" + param_name(arg, used) + "}")
        pos = m.end()
    out.append(fmt[pos:])
    return "".join(out)


def template(expr: str, calls: dict) -> str | None:
    """A {placeholder} template for a C++ string expression, or None if it isn't text."""
    expr = expr.strip()
    fmt_call = re.match(r"^(format|addReasonf)\s*\(", expr)
    if fmt_call:
        inner, _ = balanced(expr, fmt_call.end() - 1)
        args = split_top(inner, ",")
        fmt = literal(args[0]) if args else None
        return from_printf(fmt, args[1:]) if fmt is not None else None
    parts = split_top(expr, "+")
    if len(parts) > 1 or literal(expr) is not None:
        used: set[str] = set()
        out = []
        for part in parts:
            text = literal(part)
            out.append(text if text is not None else "{" + param_name(part, used) + "}")
        result = "".join(out)
        return result if re.search(r"[A-Za-z]{2,}", re.sub(r"\{\w+\}", "", result)) else None
    return None


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", lambda m: " " * len(m.group(0)), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def has_words(template_text: str | None) -> bool:
    """Prose a person reads: letters and a space (not a file name or identifier)."""
    if not template_text:
        return False
    literal_text = re.sub(r"\{\w+\}", "", template_text)
    words = bool(re.search(r"[A-Za-z]{2,}", literal_text))
    return words and (" " in literal_text.strip() or bool(re.fullmatch(r"[A-Z][a-z]+", literal_text)))


def from_call(args: list[str], spec, calls: dict) -> list[str]:
    """Templates from one call's arguments: `spec` lists text arguments, or "fmtN" for printf."""
    if isinstance(spec, str):
        i = int(spec[3:])
        fmt = literal(args[i]) if len(args) > i else None
        return [from_printf(fmt, args[i + 1 :])] if fmt is not None else []
    return [template(args[i], calls) for i in spec if len(args) > i and not re.match(r"format\s*\(", args[i].lstrip())]


def from_calls(src: str, calls: dict) -> list[str]:
    out = []
    for name, spec in calls.items():
        for m in re.finditer(rf"\b{name}\s*\(", src):
            inner, _ = balanced(src, m.end() - 1)
            out += from_call(split_top(inner, ","), spec, calls)
    return out


def from_assignments(src: str, pattern: re.Pattern) -> list[str]:
    out = []
    for m in pattern.finditer(src):
        expr = src[m.end() : src.index(";", m.end())]
        branches = re.split(r"\?|:(?!:)", expr)[1:] if "?" in expr else [expr]
        out += [template(branch.strip(" ()"), {}) for branch in branches]
    return out


def collect() -> list[tuple[str, str, str]]:
    """(area, template, source file) for every device text, first occurrence of each."""
    seen, found = set(), []
    for rel, (area, calls) in SOURCES.items():
        src = strip_comments((ROOT / rel).read_text(encoding="utf-8"))
        texts = from_calls(src, calls) + (from_assignments(src, ASSIGN[rel]) if rel in ASSIGN else [])
        for t in texts:
            if has_words(t) and t not in seen:
                seen.add(t)
                found.append((area, t, rel))
    return found


def slug(text: str) -> str:
    words = re.sub(r"\{(\w+)\}", r" \1 ", text)
    words = re.sub(r"[^A-Za-z0-9 ]+", " ", words).split()[:6]
    s = "".join(w.capitalize() if i else w.lower() for i, w in enumerate(words))
    return s[:48] or "text"


def build(en: dict, context: dict) -> tuple[dict, dict, list[tuple[str, str]]]:
    existing = {v: k for k, v in en.items() if k.startswith("device.") and isinstance(v, str)}
    entries, new_en, new_ctx = [], {k: v for k, v in en.items() if not k.startswith("device.")}, dict(context)
    for k in [k for k in new_ctx if k.startswith("device.") and k not in en]:
        new_ctx.pop(k)
    keys_in_use = set()
    for area, text, rel in collect():
        key = existing.get(text)
        if not key:
            base = f"device.{area}.{slug(text)}"
            key, n = base, 2
            while key in new_en or key in keys_in_use:
                key, n = f"{base}{n}", n + 1
        keys_in_use.add(key)
        new_en[key] = text
        if not new_ctx.get(key):
            new_ctx[key] = (
                f"Sent by the device ({rel}) and shown in the UI. Keep the {{placeholders}}: they are values the device fills in."
            )
        entries.append((key, text))
    for k in [k for k in new_ctx if k.startswith("device.") and k not in new_en]:
        new_ctx.pop(k)
    return dict(sorted(new_en.items())), dict(sorted(new_ctx.items())), sorted(entries)


def dump(data: dict) -> str:
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail if en.json is out of date")
    args = parser.parse_args()
    en = json.loads(EN.read_text(encoding="utf-8"))
    context = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.exists() else {}
    new_en, new_ctx, entries = build(en, context)
    wanted = {EN: dump(new_en)}
    if CONTEXT.exists():
        wanted[CONTEXT] = dump(new_ctx)
    stale = [p for p, text in wanted.items() if not p.exists() or p.read_text(encoding="utf-8") != text]
    if args.check:
        for p in stale:
            print(f"{p.relative_to(ROOT)} is out of date: run python3 tools/i18n/gen_device_catalog.py")
        print("OK: device catalogue up to date" if not stale else f"{len(stale)} file(s) out of date")
        return 1 if stale else 0
    for p in stale:
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(wanted[p], encoding="utf-8")
    print(f"{len(entries)} device messages; updated {len(stale)} file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
