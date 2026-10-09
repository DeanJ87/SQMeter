"""Tests for the action-result check in tools/contract-check.py (spec 013 FR-009)."""

import importlib.util
import unittest
from pathlib import Path

_spec = importlib.util.spec_from_file_location("contract_check", Path(__file__).resolve().parent / "contract-check.py")
contract_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(contract_check)


class ActionShapeTest(unittest.TestCase):
    def test_success_and_error_shapes_pass(self):
        self.assertEqual(contract_check.action_shape(200, {"success": True}), [])
        self.assertEqual(contract_check.action_shape(202, {"success": True, "pending": True}), [])
        self.assertEqual(contract_check.action_shape(400, {"error": "Hostname: ..."}), [])
        self.assertEqual(contract_check.action_shape(502, {"error": "No answer", "rawResponse": ""}), [])

    def test_mixed_or_missing_shapes_fail(self):
        self.assertTrue(contract_check.action_shape(200, {"error": "x"}))
        self.assertTrue(contract_check.action_shape(200, {"ok": True}))
        self.assertTrue(contract_check.action_shape(500, {"success": True}))
        self.assertTrue(contract_check.action_shape(409, {"error": ""}))
        self.assertTrue(contract_check.action_shape(400, None))


if __name__ == "__main__":
    unittest.main()
