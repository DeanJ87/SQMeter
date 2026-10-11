"""Tests for tools/api/openapi.py: each way the API description can drift fails (spec 028 SC-003)."""

import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import openapi  # noqa: E402 - imported after sys.path points at tools/api

REGISTRY = {
    "routes": [
        {"path": "$probe", "method": "GET", "auth": "open", "why": "probes"},
        {"path": "/api/thing", "method": "GET", "auth": "open", "why": "read-only"},
        {"path": "/api/thing", "method": "POST|PUT", "auth": "required"},
        {"path": "/api/v1", "method": "ANY", "auth": "open", "why": "Alpaca"},
    ]
}
OPERATIONS = {
    "tags": [{"name": "Things"}],
    "operations": {
        "GET /api/thing": {"tag": "Things", "summary": "Read", "description": "Reads.", "responses": {"200": "thing"}},
        "POST /api/thing": {"tag": "Things", "summary": "Change", "description": "Changes.", "responses": {"200": "action"}},
        "PUT /api/thing": {"tag": "Things", "summary": "Change", "description": "Changes.", "responses": {"200": "action"}},
    },
}
SCHEMAS = {"thing": {"type": "object"}}


def issues(registry=REGISTRY, operations=OPERATIONS, schemas=SCHEMAS, examples=None):
    return openapi.problems(registry, operations, schemas, examples or {})


class Drift(unittest.TestCase):
    def test_consistent_sources_pass(self):
        self.assertEqual(issues(), [])

    def test_route_without_an_operation_fails(self):
        registry = copy.deepcopy(REGISTRY)
        registry["routes"].append({"path": "/api/new", "method": "GET", "auth": "open", "why": "x"})
        self.assertIn("GET /api/new: in routes.json but not described in operations.json", issues(registry=registry))

    def test_operation_without_a_route_fails(self):
        operations = copy.deepcopy(OPERATIONS)
        operations["operations"]["DELETE /api/thing"] = {"tag": "Things", "summary": "Delete", "description": "Deletes.", "responses": {}}
        self.assertIn("DELETE /api/thing: described in operations.json but not in routes.json", issues(operations=operations))

    def test_method_drift_fails(self):
        registry = copy.deepcopy(REGISTRY)
        registry["routes"][2]["method"] = "POST"  # PUT dropped from the device
        self.assertIn("PUT /api/thing: described in operations.json but not in routes.json", issues(registry=registry))

    def test_unknown_schema_fails(self):
        operations = copy.deepcopy(OPERATIONS)
        operations["operations"]["GET /api/thing"]["responses"]["200"] = "renamed-thing"
        self.assertIn("GET /api/thing: unknown schema 'renamed-thing'", issues(operations=operations))

    def test_missing_example_fails(self):
        operations = copy.deepcopy(OPERATIONS)
        operations["operations"]["GET /api/thing"]["example"] = "thing"
        self.assertIn("GET /api/thing: example 'thing' missing from tools/api/examples/", issues(operations=operations))

    def test_unexcluded_non_api_route_fails(self):
        registry = copy.deepcopy(REGISTRY)
        registry["routes"].append({"path": "/other", "method": "GET", "auth": "open", "why": "x"})
        self.assertTrue(any(i.startswith("/other: not under /api/") for i in issues(registry=registry)))

    def test_auth_comes_from_the_registry(self):
        entry_open, entry_locked = REGISTRY["routes"][1], REGISTRY["routes"][2]
        read = openapi.operation("GET", "/api/thing", entry_open, OPERATIONS["operations"]["GET /api/thing"], {})
        change = openapi.operation("POST", "/api/thing", entry_locked, OPERATIONS["operations"]["POST /api/thing"], {})
        self.assertEqual(read["security"], [])
        self.assertEqual(change["security"], [{"deviceLogin": []}])
        self.assertIn("401", change["responses"])
        self.assertIn("Open: read-only.", read["description"])


class RealDocument(unittest.TestCase):
    def test_repository_document_is_current(self):
        document, found = openapi.build()
        self.assertEqual(found, [])
        self.assertEqual(openapi.OUTPUT.read_text(), openapi.render(document), "run: python3 tools/api/openapi.py")

    def test_every_registry_api_route_is_an_operation(self):
        document, _ = openapi.build()
        registry = openapi.load(openapi.REGISTRY)
        for method, path, _entry in openapi.api_routes(registry):
            self.assertIn(method.lower(), document["paths"].get(path, {}), f"{method} {path}")

    def test_alpaca_is_linked_not_described(self):
        document, _ = openapi.build()
        self.assertFalse(any(p.startswith("/api/v1") or p.startswith("/management") for p in document["paths"]))
        self.assertIn(openapi.ALPACA_DOCS, document["info"]["description"])


if __name__ == "__main__":
    unittest.main()
