"""Tests for tools/dashboard/check.py (specs/025 FR-002, FR-003).

python3 -m unittest discover -s tools/dashboard -p 'test_*.py'
"""

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check  # noqa: E402 - imported after the sys.path insert above

FILES = [check.CATALOGUE, check.INVENTORY, check.TESTS, check.ENGLISH] + [f"{check.SCHEMAS}/{n}.schema.json" for n in check.SCHEMA_NAMES]


class Repository(unittest.TestCase):
    """check() on a copy of the files it reads, broken one way at a time."""

    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        for rel in FILES:
            (self.root / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(check.ROOT / rel, self.root / rel)

    def tearDown(self):
        shutil.rmtree(self.root)

    def edit_json(self, rel, change):
        path = self.root / rel
        data = json.loads(path.read_text())
        change(data)
        path.write_text(json.dumps(data))

    def test_the_repository_passes(self):
        self.assertEqual(check.check(self.root), [])

    def test_a_new_status_field_must_be_mapped(self):
        self.edit_json(f"{check.SCHEMAS}/status.schema.json", lambda d: d["properties"].update(brandNew={"type": "number"}))
        self.assertTrue(any(p.startswith("status.brandNew isn't on the dashboard inventory") for p in check.check(self.root)))

    def test_a_new_dependency_must_be_mapped(self):
        self.edit_json(check.CATALOGUE, lambda d: d["entries"].append(dict(d["entries"][0], id="D-99")))
        self.assertTrue(any(p.startswith("deps.D-99 isn't on") for p in check.check(self.root)))

    def test_a_stale_mapping_fails(self):
        self.edit_json(check.INVENTORY, lambda d: d["notShown"].append({"match": "status.gone", "reason": "x"}))
        self.assertTrue(any("status.gone (notShown) matches no field" in p for p in check.check(self.root)))

    def test_a_shown_entry_needs_a_test(self):
        tests = self.root / check.TESTS
        tests.write_text(tests.read_text().replace("inventory: imaging-app", "inventory: something-else"))
        self.assertTrue(any(p.startswith("shown entry imaging-app has no test") for p in check.check(self.root)))

    def test_a_shown_entry_needs_its_label(self):
        self.edit_json(check.INVENTORY, lambda d: d["shown"][0].update(label="glance.nope"))
        self.assertTrue(any("label glance.nope isn't in" in p for p in check.check(self.root)))

    def test_globs_cover_subtrees(self):
        self.assertTrue(check.matches("status.diagnostics.**", "status.diagnostics.rain.state"))
        self.assertTrue(check.matches("status.heapStages.**", "status.heapStages[].free"))
        self.assertFalse(check.matches("status.diagnostics.**", "status.diagnosticsX"))
        self.assertFalse(check.matches("status.wifi", "status.wifi.ip"))


if __name__ == "__main__":
    unittest.main()
