#!/usr/bin/env python3
"""Check a real SQMeter against the documented response formats - the same
JSON Schemas the demo is tested against (specs/016-demo-device-emulation/
contracts/schemas). Standard library only.

  python3 tools/contract-check.py http://192.168.1.128 [user:password]

Also sends a few actions that change nothing (bad settings, an empty MQTT
test, an RG-15 read) and checks every action answers in the one shape
(spec 013 FR-009): 2xx with {"success": true, ...}, or 4xx/5xx with
{"error": "..."}. Pass the login when password protection is on.
"""

import glob
import json
import os
import sys
import base64
import urllib.error
import urllib.request

SCHEMAS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "specs", "016-demo-device-emulation", "contracts", "schemas")

ENDPOINTS = [
    ("/api/sensors", "readings"),
    ("/api/status", "status"),
    ("/api/safety", "safety"),
    ("/api/settings/effective", "settings-effective"),
    ("/api/config", "config"),
    ("/api/i18n", "i18n"),
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
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "string": lambda v: isinstance(v, str),
    "array": lambda v: isinstance(v, list),
    "object": lambda v: isinstance(v, dict),
}


def validate_object(value, schema, path, errors):
    """Required keys present; every key either in the contract or allowed extra."""
    props = schema.get("properties", {})
    errors.extend(f"{path}/{key}: missing" for key in schema.get("required", []) if key not in value)
    extra = schema.get("additionalProperties", True)
    for key, item in value.items():
        if key in props:
            validate(item, props[key], f"{path}/{key}", errors)
        elif extra is False:
            errors.append(f"{path}/{key}: not in the contract")
        elif isinstance(extra, dict):
            validate(item, extra, f"{path}/{key}", errors)


def load_schema(path):
    with open(path) as handle:
        return json.load(handle)


def validate(value, schema, path, errors):
    """The subset of JSON Schema the generated schemas use."""
    types = schema.get("type")
    if types:
        allowed = types if isinstance(types, list) else [types]
        if not any(TYPES[t](value) for t in allowed):
            errors.append(f"{path or '/'}: expected {'/'.join(allowed)}, got {type(value).__name__}")
            return
    if isinstance(value, dict):
        validate_object(value, schema, path, errors)
    if isinstance(value, list) and "items" in schema:
        for i, item in enumerate(value):
            validate(item, schema["items"], f"{path}[{i}]", errors)


# Actions that change nothing on the device: (method, path, body).
ACTIONS = [
    ("POST", "/api/config", '{"wifi":{"hostname":"-bad-"}}'),
    ("POST", "/api/mqtt/test", "{}"),
    ("POST", "/api/wifi/connect", "{}"),
    ("POST", "/api/sensors/rg15/test", None),
]


def action_shape(status, document):
    """Problems with an action's answer, or [] if it has the one shape."""
    if not isinstance(document, dict):
        return [f"HTTP {status}: answer is not a JSON object"]
    if 200 <= status < 300:
        if document.get("success") is not True or "error" in document:
            return [f'HTTP {status}: expected {{"success": true, ...}}']
        return []
    if not isinstance(document.get("error"), str) or not document["error"] or document.get("success") is True:
        return [f'HTTP {status}: expected {{"error": "message"}}']
    return []


def send(base, method, path, body, auth):
    data = body.encode() if body is not None else b""
    request = urllib.request.Request(base + path, data=data, method=method)
    request.add_header("Content-Type", "application/json")
    if auth:
        request.add_header("Authorization", "Basic " + base64.b64encode(auth.encode()).decode())
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as e:
        text = e.read()
        try:
            return e.code, json.loads(text or b"null")
        except ValueError:
            return e.code, None


def check_actions(base, auth):
    failed = 0
    for method, path, body in ACTIONS:
        try:
            status, document = send(base, method, path, body, auth)
        except Exception as e:  # noqa: BLE001 - report and carry on
            print(f"FAIL {method} {path}: {e}")
            failed += 1
            continue
        if status == 401:
            print(f"skip {method} {path}: password protection is on - pass user:password")
            continue
        problems = action_shape(status, document)
        print(("ok   " if not problems else "FAIL ") + f"{method} {path} ({status})")
        for problem in problems:
            print("       " + problem)
        failed += bool(problems)
    return failed


def main():
    if len(sys.argv) not in (2, 3):
        print(__doc__)
        sys.exit(2)
    base = sys.argv[1].rstrip("/")
    auth = sys.argv[2] if len(sys.argv) == 3 else None
    schemas = {os.path.basename(p)[: -len(".schema.json")]: load_schema(p) for p in glob.glob(os.path.join(SCHEMAS, "*.schema.json"))}
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
    failed += check_actions(base, auth)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
