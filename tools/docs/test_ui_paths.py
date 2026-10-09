"""Tests for tools/docs/ui_paths.py (spec 026, DS-27 docs drift).

python3 -m unittest discover -s tools/docs -p 'test_*.py'
"""

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import ui_paths


class UiPathsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        (root / "web/src/i18n").mkdir(parents=True)
        (root / "web/src/components").mkdir(parents=True)
        (root / "docs").mkdir()
        (root / "web/src/i18n/en.json").write_text(
            json.dumps({"nav.settings": "Settings", "tabs.safety": "Safety", "tabs.network": "Network", "card.mqtt.publish": "Publish"}),
            encoding="utf-8",
        )
        (root / "web/src/components/Safety.tsx").write_text('<SettingsCard title="ASCOM Alpaca">', encoding="utf-8")
        self.doc = root / "docs/page.md"
        patches = {
            "ROOT": root,
            "ENGLISH": root / "web/src/i18n/en.json",
            "COMPONENTS": root / "web/src/components",
            "DOCS": [self.doc],
        }
        for name, value in patches.items():
            patcher = mock.patch.object(ui_paths, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def tearDown(self):
        self.tmp.cleanup()

    def test_known_labels_and_literal_titles_pass(self):
        self.doc.write_text("Turn on **Settings → Safety → ASCOM Alpaca**, then **Settings → Network**.\n", encoding="utf-8")
        self.assertEqual(ui_paths.findings(), [])

    def test_a_renamed_card_is_reported(self):
        self.doc.write_text("Open **Settings → Safety → Alpaca**.\n", encoding="utf-8")
        found = ui_paths.findings()
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]["line"], 1)
        self.assertIn("'Alpaca'", found[0]["message"])

    def test_case_is_ignored(self):
        self.doc.write_text("**Settings → network → publish**\n", encoding="utf-8")
        self.assertEqual(ui_paths.findings(), [])


if __name__ == "__main__":
    unittest.main()
