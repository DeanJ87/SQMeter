#!/usr/bin/env python3
"""Check a real SQMeter against the documented response formats - the same
JSON Schemas the demo is tested against (specs/016-demo-device-emulation/
contracts/schemas). Standard library only.

  python3 tools/contract-check.py http://192.168.1.128
"""
import glob
import json
import os
import sys
import urllib.request

SCHEMAS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "specs", "016-demo-device-emulation", "contracts", "schemas")

ENDPOINTS = [
    ("/api/sensors", "readings"),
    ("/api/status", "status"),
    ("/api/safety", "safety"),
    ("/api/config", "config"),
    ("/api/alerts/recent", "alerts-recent"),
    ("/api/alerts/armed", "alerts-armed"),
    ("/api/safety/history", "safety-history"),
    ("/management/v1/description", "alpaca-description"),
    ("/management/v1/configureddevices", "alpaca-configured-devices"),
    ("/api/v1/safetymonitor/0/devicestate", "alpaca-devicestate"),
    ("/api/v1/observingconditions/0/devicestate", "alpaca-devicestate"),
]

TYPES = {
    "null": lambda v: v is None,
    "boolean": lambda v: isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "string": lambda v: isinstance(v, str),
    "array": lambda v: isinstance(v, list),
    "object": lambda v: isinstance(v, dict),
}


def validate(value, schema, path, errors):
    """The subset of JSON Schema the generated schemas use."""
    types = schema.get("type")
    if types:
        allowed = types if isinstance(types, list) else [types]
        if not any(TYPES[t](value) for t in allowed):
            errors.append(f"{path or '/'}: expected {'/'.join(allowed)}, got {type(value).__name__}")
            return
    if isinstance(value, dict):
        props = schema.get("properties", {})
        for key in schema.get("required", []):
            if key not in value:
                errors.append(f"{path}/{key}: missing")
        extra = schema.get("additionalProperties", True)
        for key, item in value.items():
            if key in props:
                validate(item, props[key], f"{path}/{key}", errors)
            elif extra is False:
                errors.append(f"{path}/{key}: not in the contract")
            elif isinstance(extra, dict):
                validate(item, extra, f"{path}/{key}", errors)
    if isinstance(value, list) and "items" in schema:
        for i, item in enumerate(value):
            validate(item, schema["items"], f"{path}[{i}]", errors)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    base = sys.argv[1].rstrip("/")
    schemas = {os.path.basename(p)[: -len(".schema.json")]: json.load(open(p)) for p in glob.glob(os.path.join(SCHEMAS, "*.schema.json"))}
    failed = 0
    for path, name in ENDPOINTS:
        try:
            with urllib.request.urlopen(base + path, timeout=15) as response:
                document = json.load(response)
        except Exception as e:  # noqa: BLE001 - report and carry on
            print(f"FAIL {path}: {e}")
            failed += 1
            continue
        errors = []
        validate(document, schemas[name], "", errors)
        print(("ok   " if not errors else "FAIL ") + path)
        for error in errors[:20]:
            print("       " + error)
        failed += bool(errors)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
