#!/usr/bin/env python3
"""Every HTTP route the device serves is declared, with its auth, and documented.

Reads the route registrations in src/WebServer*.cpp and src/LanguagePack.cpp (``server.on(...)`` and
``new AsyncCallbackJsonWebHandler(...)``), works out whether each one calls
``requireAuth`` (directly, or in a handler it calls), and compares that with
tools/api/routes.json. Fails when a route is missing from the registry, the
registry is out of date, the auth differs, or an /api/ route isn't in
docs/api/rest.md (specs 011 FR-006, 014 FR-005).

Usage: python3 tools/api/routes.py [--list]
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Files that register routes: the web server's sources, and the modules it hands the server to.
SOURCES = [*sorted((ROOT / "src").glob("WebServer*.cpp")), ROOT / "src" / "LanguagePack.cpp"]
REGISTRY = ROOT / "tools" / "api" / "routes.json"
REST_DOCS = ROOT / "docs" / "api" / "rest.md"

REGISTRATION = re.compile(r"server\.on\(|new AsyncCallbackJsonWebHandler\(")
STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")


def read_sources():
    return "\n".join(path.read_text() for path in SOURCES)


def after_literal(text, i):
    """Index just past the string ("...") or character ('x') literal starting at i."""
    if text[i] == "'":
        return i + (4 if text[i + 1] == "\\" else 3)
    i += 1
    while i < len(text) and text[i] != '"':
        i += 2 if text[i] == "\\" else 1
    return i + 1


def matching_close(text, open_index, open_char="(", close_char=")"):
    """Index of the bracket closing the one at open_index, skipping string literals."""
    depth = 0
    i = open_index
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i = after_literal(text, i)
            continue
        depth += (c == open_char) - (c == close_char)
        if depth == 0 and c == close_char:
            return i
        i += 1
    raise ValueError(f"unbalanced {open_char} at {open_index}")


def definitions(text):
    """Bodies of WebServer/LanguagePack member functions and local `auto name = [...]` lambdas."""
    bodies = {}
    for m in re.finditer(r"(?:WebServer|LanguagePack)::(\w+)\([^;{]*\)\s*(?:const\s*)?\{", text):
        start = m.end() - 1
        bodies[m.group(1)] = text[start : matching_close(text, start, "{", "}") + 1]
    for m in re.finditer(r"auto\s+(\w+)\s*=\s*\[", text):
        brace = text.index("{", m.end())
        bodies[m.group(1)] = text[brace : matching_close(text, brace, "{", "}") + 1]
    return bodies


def method_of(text, args, kind, var):
    if kind == "json":
        set_method = re.search(rf"{re.escape(var)}->setMethod\(([^)]*)\)", text) if var else None
        methods = re.findall(r"HTTP_([A-Z]+)", set_method.group(1)) if set_method else ["POST"]
    else:
        first = re.search(r"HTTP_[A-Z_|\s]+", args)
        methods = re.findall(r"HTTP_([A-Z]+)", first.group(0)) if first else ["ANY"]
    return "|".join(sorted(methods))


def discover(text):
    bodies = definitions(text)
    routes = []
    for m in REGISTRATION.finditer(text):
        if "//" in text[text.rfind("\n", 0, m.start()) + 1 : m.start()]:
            continue  # mentioned in a comment
        open_paren = m.end() - 1
        args = text[open_paren : matching_close(text, open_paren) + 1]
        literal = STRING.match(args[1:].lstrip())
        path = literal.group(1) if literal else "$" + re.match(r"\(\s*(\w+)", args).group(1)
        kind = "json" if "Json" in m.group(0) else "on"
        var = None
        if kind == "json":
            assign = re.search(r"(\w+)\s*=\s*$", text[max(0, m.start() - 80) : m.start()])
            var = assign.group(1) if assign else None
        region = args + "".join(bodies.get(name, "") for name in set(CALL.findall(args)))
        routes.append(
            {
                "path": path,
                "method": method_of(text, args, kind, var),
                "auth": "required" if "requireAuth" in region else "open",
            }
        )
    return routes


def key(route):
    return (route["path"], route["method"])


def check(source_text=None, registry=None, docs=None):
    source_text = read_sources() if source_text is None else source_text
    registry = json.loads(REGISTRY.read_text()) if registry is None else registry
    docs = REST_DOCS.read_text() if docs is None else docs
    found = {key(r): r for r in discover(source_text)}
    declared = {key(r): r for r in registry["routes"]}
    problems = []
    for k, route in sorted(found.items()):
        entry = declared.get(k)
        if entry is None:
            problems.append(f"{route['method']} {route['path']}: not in tools/api/routes.json - declare it with its auth and why")
        elif entry["auth"] != route["auth"]:
            problems.append(
                f"{route['method']} {route['path']}: registry says auth {entry['auth']}, code is {route['auth']} "
                "(requireAuth in the handler decides)"
            )
        documented = re.search(re.escape(route["path"]) + r"(?![\w/-])", docs)
        if route["path"].startswith("/api/") and not route["path"].startswith("/api/v1") and not documented:
            problems.append(f"{route['method']} {route['path']}: not documented in docs/api/rest.md")
    for k in sorted(set(declared) - set(found)):
        problems.append(f"{k[1]} {k[0]}: in tools/api/routes.json but no longer served - remove it")
    for entry in declared.values():
        if entry["auth"] == "open" and not entry.get("why"):
            problems.append(f"{entry['method']} {entry['path']}: open routes need a `why`")
    return problems


def main(argv):
    if "--list" in argv:
        print(json.dumps(discover(read_sources()), indent=2))
        return 0
    problems = check()
    for p in problems:
        print(f"API-ROUTES: {p}")
    if problems:
        print(f"{len(problems)} problem(s). Every route the device serves is declared in tools/api/routes.json.")
        return 1
    print(f"OK: {len(discover(read_sources()))} routes declared, auth matches, /api/ routes documented")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
