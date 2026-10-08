#!/usr/bin/env python3
"""Generate the device's JSON Schemas (specs/016-demo-device-emulation/contracts/schemas)
from sample documents captured from a real SQMeter and from the demo's device core.

  python3 tools/contracts/generate_schemas.py <samples-dir>

Samples are named <source>-<document>.json (source = device | demo). A key is
required when every sample of that object has it; any key no sample has is
rejected (additionalProperties: false), so a renamed or added field shows up.
Review the diff before committing: the schemas are the contract."""

import glob
import json
import os
import sys

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "specs", "016-demo-device-emulation", "contracts", "schemas")

# document name -> schema file; status is built from device samples only (its
# hardware sections are emulated by the demo and must match the device).
DOCUMENTS = {
    "sensors": "readings",
    "status": "status",
    # The demo core's decision parts of the status document: they add what
    # optional fields can hold, but don't make anything required.
    "status_partial": "status",
    "safety": "safety",
    "settings_effective": "settings-effective",
    "config": "config",
    "alerts_recent": "alerts-recent",
    "safety_history": "safety-history",
    "management_v1_description": "alpaca-description",
    "management_v1_configureddevices": "alpaca-configured-devices",
    "api_v1_safetymonitor_0_devicestate": "alpaca-devicestate",
    "api_v1_observingconditions_0_devicestate": "alpaca-devicestate",
}

# Objects keyed by data (a variable set of keys): only their values are described.
OPEN_MAPS = {("channels",)}

# Readings groups carry values only while their sensor is ok (contract
# specs/013-data-interfaces/contracts/readings.md), so only "status" is required.
READING_GROUPS = {"light", "sky", "environment", "infrared", "clouds", "gps", "rain", "wind"}


def type_of(value):
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, (int, float)):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    return "object"


def merge(samples, path=(), partial=()):
    types = sorted({type_of(s) for s in samples})
    schema = {"type": types[0] if len(types) == 1 else types}
    objects = [s for s in samples if isinstance(s, dict)]
    if objects:
        if path[-1:] and (path[-1],) in OPEN_MAPS:
            values = [v for o in objects for v in o.values()]
            if values:
                schema["additionalProperties"] = merge(values, path + ("*",))
            return schema
        full = [o for o in objects if not any(o is p for p in partial)]
        keys = sorted({k for o in objects for k in o})
        schema["properties"] = {
            k: merge([o[k] for o in objects if k in o], path + (k,), tuple(o[k] for o in partial if isinstance(o, dict) and k in o))
            for k in keys
        }
        schema["required"] = [k for k in keys if all(k in o for o in full)]
        if len(path) == 1 and path[0] in READING_GROUPS and "status" in keys:
            schema["required"] = ["status"]
        schema["additionalProperties"] = False
    arrays = [s for s in samples if isinstance(s, list)]
    items = [i for a in arrays for i in a]
    if arrays and items:
        schema["items"] = merge(items, path + ("[]",))
    return schema


def main():
    samples_dir = sys.argv[1]
    grouped = {}
    for path in glob.glob(os.path.join(samples_dir, "*.json")):
        name = os.path.basename(path)[:-5]
        source, _, document = name.partition("-")
        if document not in DOCUMENTS or (document == "status" and not source.startswith("device")):
            continue
        with open(path) as f:
            sample = json.load(f)
        entry = grouped.setdefault(DOCUMENTS[document], {"samples": [], "partial": []})
        entry["samples"].append(sample)
        if document.endswith("_partial"):
            entry["partial"].append(sample)
    os.makedirs(OUT, exist_ok=True)
    for schema_name, entry in sorted(grouped.items()):
        docs = entry["samples"]
        schema = {"$schema": "http://json-schema.org/draft-07/schema#", "title": schema_name, **merge(docs, (), tuple(entry["partial"]))}
        with open(os.path.join(OUT, schema_name + ".schema.json"), "w") as f:
            json.dump(schema, f, indent=2, sort_keys=False)
            f.write("\n")
        print(f"{schema_name}: {len(docs)} samples")


if __name__ == "__main__":
    main()
