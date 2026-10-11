#!/usr/bin/env python3
"""Builds and checks the OpenAPI 3.1 description of the device's REST API (spec 028).

Sources, in order of authority:
  tools/api/routes.json       every route, its methods and auth (checked against src/ by routes.py)
  tools/api/operations.json   summary, description, parameters, bodies and responses per operation
  specs/016-demo-device-emulation/contracts/schemas/   the documents' JSON Schemas
  tools/api/examples/         examples made by the demo (web/src/demo/__tests__/openapi-examples.test.ts)

Output: docs/api/openapi.json (committed; the docs site and the demo's API explorer read it).

Usage:
  python3 tools/api/openapi.py            write docs/api/openapi.json
  python3 tools/api/openapi.py --check    fail on drift or a stale document (CI)
  python3 tools/api/openapi.py --out FILE --version 0.3.2-beta.1   a release's copy
"""

import argparse
import copy
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "tools" / "api" / "routes.json"
OPERATIONS = ROOT / "tools" / "api" / "operations.json"
SCHEMAS = ROOT / "specs" / "016-demo-device-emulation" / "contracts" / "schemas"
EXAMPLES = ROOT / "tools" / "api" / "examples"
OUTPUT = ROOT / "docs" / "api" / "openapi.json"
VERSION_H = ROOT / "include" / "version.h"

# Registry entries that aren't REST operations, and why (spec 028 edge cases, FR-007).
EXCLUDED = {
    "$probe": "captive-portal probes from phones and laptops, not an API",
    "/setup": "ASCOM Alpaca setup pages (redirect to Settings)",
    "/management": "ASCOM Alpaca management API: described by the ASCOM Alpaca standard",
    "/api/v1": "ASCOM Alpaca device API: described by the ASCOM Alpaca standard",
}
ALPACA_DOCS = "https://ascom-standards.org/api/"
DOCS_REFERENCE = "https://sqmeter.dev/api/rest/"
HTTP_STATUS = {
    "200": "OK",
    "202": "Accepted",
    "400": "Bad request",
    "401": "The password is needed",
    "409": "Not now",
    "500": "Device failure",
    "502": "A sensor, broker or GitHub didn't answer",
}


def load(path):
    return json.loads(path.read_text())


def firmware_version():
    match = re.search(r'#define FIRMWARE_VERSION "([^"]+)"', VERSION_H.read_text())
    return match.group(1) if match else "0.0.0"


def schema_name(file_stem):
    """readings -> Readings, safety-history -> SafetyHistory."""
    return "".join(part.capitalize() for part in file_stem.split("-"))


def without_required(schema):
    """The partial-update form of a schema: every field optional, at every level."""
    out = copy.deepcopy(schema)

    def strip(node):
        if isinstance(node, dict):
            node.pop("required", None)
            for value in node.values():
                strip(value)
        elif isinstance(node, list):
            for value in node:
                strip(value)

    strip(out)
    return out


def contract_schemas():
    schemas = {}
    for path in sorted(SCHEMAS.glob("*.schema.json")):
        schema = load(path)
        schema.pop("$schema", None)
        schemas[path.name[: -len(".schema.json")]] = schema
    return schemas


def api_routes(registry):
    """(method, path, entry) for every REST operation in the registry."""
    routes = []
    for entry in registry["routes"]:
        if entry["path"] in EXCLUDED:
            continue
        for method in entry["method"].split("|"):
            routes.append((method, entry["path"], entry))
    return routes


def problems(registry, operations, schemas, examples):
    """Every way the registry, the operation details, the schemas and the examples disagree."""
    found = []
    for entry in registry["routes"]:
        path = entry["path"]
        if path not in EXCLUDED and not path.startswith("/api/"):
            found.append(f"{path}: not under /api/ and not excluded by rule - add it to EXCLUDED with a reason")
    wanted = {f"{method} {path}" for method, path, _ in api_routes(registry)}
    described = set(operations["operations"])
    found += [f"{key}: in routes.json but not described in operations.json" for key in sorted(wanted - described)]
    found += [f"{key}: described in operations.json but not in routes.json" for key in sorted(described - wanted)]
    known = set(schemas) | {"action", "error", "config-update"}
    for key, op in sorted(operations["operations"].items()):
        refs = [r for r in op.get("responses", {}).values() if isinstance(r, str)]
        body = op.get("requestBody", {})
        if "schema" in body:
            refs.append(body["schema"])
        found += [f"{key}: unknown schema '{ref}'" for ref in refs if ref not in known]
        if "example" in op and op["example"] not in examples:
            found.append(f"{key}: example '{op['example']}' missing from tools/api/examples/")
        if not op.get("summary") or not op.get("description"):
            found.append(f"{key}: needs a summary and a description")
    return found


def response_object(status, ref, example):
    if isinstance(ref, dict) and "text" in ref:
        content = {"text/plain": {"schema": {"type": "string", "enum": ref["enum"]}, "example": ref["enum"][1]}}
        return {"description": ref["text"], "content": content}
    if isinstance(ref, dict) and "inline" in ref:
        return {"description": HTTP_STATUS[status], "content": {"application/json": {"schema": ref["inline"]}}}
    target = {"action": "ActionResult", "error": "Error"}.get(ref, schema_name(ref))
    media = {"schema": {"$ref": f"#/components/schemas/{target}"}}
    if example is not None and status in ("200", "202"):
        media["example"] = example
    return {"description": HTTP_STATUS[status], "content": {"application/json": media}}


def request_body(op):
    body = op.get("requestBody")
    if "upload" in op:
        upload = op["upload"]
        schema = {
            "type": "object",
            "properties": {
                upload["field"]: {"type": "string", "contentMediaType": "application/octet-stream", "description": upload["about"]}
            },
            "required": [upload["field"]],
        }
        return {"required": True, "content": {"multipart/form-data": {"schema": schema}}}
    if not body:
        return None
    schema = body["inline"] if "inline" in body else {"$ref": f"#/components/schemas/{schema_name(body['schema'])}"}
    media = {"schema": schema}
    if "example" in body:
        media["example"] = body["example"]
    return {"required": True, "content": {"application/json": media}}


def operation(method, path, entry, op, examples):
    open_route = entry["auth"] == "open"
    description = op["description"]
    if open_route and entry.get("why"):
        description += f"\n\nOpen: {entry['why']}."
    elif not open_route:
        description += "\n\nNeeds the password when protection is on."
    example = examples.get(op.get("example")) if op.get("example") else None
    out = {
        "operationId": (method.lower() + "_" + path.removeprefix("/api/").replace("/", "_").replace("-", "_")),
        "tags": [op["tag"]],
        "summary": op["summary"],
        "description": description,
        "security": [] if open_route else [{"deviceLogin": []}],
        "responses": {status: response_object(status, ref, example) for status, ref in op["responses"].items()},
    }
    if not open_route:
        out["responses"].setdefault("401", {"description": HTTP_STATUS["401"]})
    if op.get("parameters"):
        out["parameters"] = op["parameters"]
    body = request_body(op)
    if body:
        out["requestBody"] = body
    return out


INFO_DESCRIPTION = (
    "Every `/api/*` route an SQMeter serves on port 80. Generated from the firmware's route "
    "registry and its contract schemas, so it matches the device.\n\n"
    "**The same in every language**: numbers are JSON numbers with `.` as the decimal point, "
    "units and field names are never translated, and timestamps are Unix seconds or ISO 8601. "
    "Text for people (`reasons`, alert `title` and `message`, `error`) may be reworded between "
    "releases: key on `safe`, `reasonFlags` and the alert `event`.\n\n"
    "**Password**: when protection is on, routes that change something need HTTP Basic auth "
    "with the device's username and password.\n\n"
    "**ASCOM Alpaca** (`/api/v1/*`, `/management/*`) follows the ASCOM Alpaca standard and isn't "
    f"repeated here: the device is a SafetyMonitor and an ObservingConditions device. See {ALPACA_DOCS}."
)
SERVERS = [
    {
        "url": "http://{device}",
        "description": "Your SQMeter",
        "variables": {"device": {"default": "sqmeter.local", "description": "The device's address or .local name"}},
    }
]
SECURITY_SCHEMES = {
    "deviceLogin": {
        "type": "http",
        "scheme": "basic",
        "description": "The device's username and password, when protection is on.",
    }
}
PARAMETERS = {
    "source": {
        "name": "source",
        "in": "query",
        "description": "`ui` when the web UI sends it; shown as the reason.",
        "schema": {"type": "string"},
    }
}
ERROR_SCHEMA = {
    "title": "Error",
    "type": "object",
    "properties": {"error": {"type": "string", "description": "What went wrong, in English."}},
    "required": ["error"],
}


def component_schemas(schemas):
    """The contract schemas, plus the partial-update form of the settings and the error document."""
    components = {schema_name(name): schema for name, schema in schemas.items()}
    components["ConfigUpdate"] = without_required(schemas["config"])
    components["ConfigUpdate"]["title"] = "Settings (partial update)"
    components["Error"] = ERROR_SCHEMA
    return dict(sorted(components.items()))


def build(version=None):
    registry, operations, schemas = load(REGISTRY), load(OPERATIONS), contract_schemas()
    examples = {p.stem: load(p) for p in sorted(EXAMPLES.glob("*.json"))}
    issues = problems(registry, operations, schemas, examples)
    if issues:
        return None, issues

    paths = {}
    for method, path, entry in api_routes(registry):
        op = operations["operations"][f"{method} {path}"]
        paths.setdefault(path, {})[method.lower()] = operation(method, path, entry, op, examples)

    alpaca_tag = {"name": "Alpaca", "description": "ASCOM Alpaca: see the standard.", "externalDocs": {"url": ALPACA_DOCS}}
    document = {
        "openapi": "3.1.0",
        "info": {
            "title": "SQMeter REST API",
            "version": version or firmware_version(),
            "summary": "The REST API of an SQMeter sky-quality and safety monitor.",
            "description": INFO_DESCRIPTION,
            "license": {"name": "MIT", "identifier": "MIT"},
        },
        "externalDocs": {"description": "SQMeter REST API guide", "url": DOCS_REFERENCE},
        "servers": SERVERS,
        "tags": operations["tags"] + [alpaca_tag],
        "paths": paths,
        "components": {"securitySchemes": SECURITY_SCHEMES, "parameters": PARAMETERS, "schemas": component_schemas(schemas)},
    }
    return document, []


def render(document):
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def validate(document):
    """OpenAPI 3.1 validation when openapi-spec-validator is installed (CI installs it)."""
    try:
        from openapi_spec_validator import validate as spec_validate
    except ImportError:
        return ["openapi-spec-validator isn't installed - pip install -r tools/quality/requirements.txt"]
    try:
        spec_validate(document)
    except Exception as e:  # noqa: BLE001 - report the validator's message
        return [f"not valid OpenAPI 3.1: {e}".splitlines()[0]]
    return []


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail on drift or a stale docs/api/openapi.json")
    parser.add_argument("--out", type=Path, default=OUTPUT)
    parser.add_argument("--version", help="info.version (default: include/version.h)")
    parser.add_argument("--json", action="store_true", help="--check, printing problems as [{file, line, message}] (quality gate)")
    args = parser.parse_args(argv)

    if args.json:
        args.check = True
    document, issues = build(args.version)
    if not issues:
        issues = validate(document)
    if not issues and args.check:
        current = OUTPUT.read_text() if OUTPUT.exists() else ""
        if current != render(document):
            issues = ["docs/api/openapi.json is out of date - run: python3 tools/api/openapi.py"]
    if args.json:
        print(json.dumps([{"file": "docs/api/openapi.json", "line": 1, "message": issue} for issue in issues]))
        return 0
    if issues:
        for issue in issues:
            print(f"OPENAPI: {issue}")
        return 1
    if not args.check:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(render(document))
        print(f"wrote {args.out.relative_to(ROOT) if args.out.is_relative_to(ROOT) else args.out}: {len(document['paths'])} paths")
    else:
        print(f"OK: {sum(len(v) for v in document['paths'].values())} operations match the route registry and schemas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
