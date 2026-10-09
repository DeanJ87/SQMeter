"""Tests for tools/api/routes.py: each way a route can drift fails."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import routes  # noqa: E402 - imported after sys.path points at tools/api

SOURCE = """
void WebServer::setupAPIRoutes()
{
    // a comment mentioning server.on("/not/a/route") is ignored
    server.on("/api/open", HTTP_GET, [this](AsyncWebServerRequest *request) { handleOpen(request); });
    server.on("/api/locked", HTTP_POST, [this](AsyncWebServerRequest *request) { handleLocked(request); });
    AsyncCallbackJsonWebHandler *h = new AsyncCallbackJsonWebHandler(
        "/api/json", [this](AsyncWebServerRequest *request, JsonVariant &json) { if (!requireAuth(request)) return; });
    h->setMethod(HTTP_POST | HTTP_PUT);
}
void WebServer::handleOpen(AsyncWebServerRequest *request) { request->send(200); }
void WebServer::handleLocked(AsyncWebServerRequest *request)
{
    if (!requireAuth(request))
        return;
}
"""

DOCS = "`GET /api/open` `POST /api/locked` `POST /api/json`"


def registry(*overrides):
    entries = [
        {"path": "/api/open", "method": "GET", "auth": "open", "why": "read-only"},
        {"path": "/api/locked", "method": "POST", "auth": "required"},
        {"path": "/api/json", "method": "POST|PUT", "auth": "required"},
    ]
    for path, field, value in overrides:
        next(e for e in entries if e["path"] == path)[field] = value
    return {"routes": entries}


class RoutesTest(unittest.TestCase):
    def test_discovers_paths_methods_and_auth(self):
        found = {(r["path"], r["method"]): r["auth"] for r in routes.discover(SOURCE)}
        self.assertEqual(
            found,
            {("/api/open", "GET"): "open", ("/api/locked", "POST"): "required", ("/api/json", "POST|PUT"): "required"},
        )

    def test_consistent_registry_passes(self):
        self.assertEqual(routes.check(SOURCE, registry(), DOCS), [])

    def test_undeclared_route_fails(self):
        reg = registry()
        reg["routes"] = [e for e in reg["routes"] if e["path"] != "/api/locked"]
        self.assertTrue(any("not in tools/api/routes.json" in p for p in routes.check(SOURCE, reg, DOCS)))

    def test_auth_mismatch_fails(self):
        problems = routes.check(SOURCE, registry(("/api/locked", "auth", "open"), ("/api/locked", "why", "x")), DOCS)
        self.assertTrue(any("registry says auth open" in p for p in problems))

    def test_removed_route_fails(self):
        reg = registry()
        reg["routes"].append({"path": "/api/gone", "method": "GET", "auth": "open", "why": "x"})
        self.assertTrue(any("no longer served" in p for p in routes.check(SOURCE, reg, DOCS)))

    def test_undocumented_route_fails(self):
        problems = routes.check(SOURCE, registry(), "`GET /api/open`")
        self.assertTrue(any("/api/locked: not documented" in p for p in problems))

    def test_open_route_needs_a_reason(self):
        problems = routes.check(SOURCE, registry(("/api/open", "why", "")), DOCS)
        self.assertTrue(any("need a `why`" in p for p in problems))


if __name__ == "__main__":
    unittest.main()
