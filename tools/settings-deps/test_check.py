#!/usr/bin/env python3
"""Tests for check.py (specs/020-settings-dependencies, US4).

python3 tools/settings-deps/test_check.py
"""

import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check  # noqa: E402 - importable only once its directory is on the path

COPY = ["lib", "test", "src", "include", "web/src", "tools/demo-core/bridge.cpp", "docs/reference/settings-dependencies.md"]


class UndeclaredDependencies(unittest.TestCase):
    def test_flags_a_switch_gated_on_another_sections_switch(self):
        code = "if (cfg.alerts.mqttEnabled && cfg.mqtt.enabled)\n    publish();\n"
        problems = check.undeclared_dependencies("src/X.cpp", code)
        self.assertEqual(1, len(problems))
        self.assertIn("src/X.cpp:1", problems[0])
        self.assertIn("alerts and mqtt", problems[0])

    def test_a_marker_declares_it(self):
        code = "// dep: D-01\nif (cfg.alerts.mqttEnabled && cfg.mqtt.enabled)\n    publish();\n"
        self.assertEqual([], check.undeclared_dependencies("src/X.cpp", code))
        same_line = "if (cfg.alerts.mqttEnabled && cfg.mqtt.enabled) // dep: D-01\n"
        self.assertEqual([], check.undeclared_dependencies("src/X.cpp", same_line))

    def test_one_section_is_not_a_dependency(self):
        self.assertEqual([], check.undeclared_dependencies("src/X.cpp", "if (cfg.wind.enabled && cfg.wind.directionEnabled)\n"))

    def test_markers_list_every_id(self):
        self.assertEqual({"D-01", "D-02"}, check.markers("x(); // dep: D-01 D-02\n"))


class WholeRepository(unittest.TestCase):
    """check() on a copy of the repository, broken in one place at a time."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        for rel in COPY:
            src = os.path.join(check.ROOT, rel)
            dst = os.path.join(self.root, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.isdir(src):
                shutil.copytree(src, dst, ignore=shutil.ignore_patterns("node_modules", "core", "*.wasm"))
            else:
                shutil.copy(src, dst)

    def tearDown(self):
        shutil.rmtree(self.root)

    def test_the_repository_passes(self):
        self.assertEqual([], check.check(self.root))

    def test_a_new_setting_with_an_undeclared_dependency_fails_and_is_named(self):
        path = os.path.join(self.root, "src", "NewFeature.cpp")
        with open(path, "w") as f:
            f.write("void tick(const Config &cfg)\n{\n    if (cfg.ota.enabled && cfg.wifi.mdns && cfg.rain.enabled)\n        go();\n}\n")
        problems = check.check(self.root)
        self.assertTrue(any("src/NewFeature.cpp:3" in p for p in problems), problems)
        # Declaring it with a catalogue ID passes.
        with open(path, "w") as f:
            f.write("void tick(const Config &cfg)\n{\n    if (cfg.ota.enabled && cfg.rain.enabled) // dep: D-24\n        go();\n}\n")
        self.assertEqual([], check.check(self.root))

    def test_an_entry_without_tests_fails_and_is_named(self):
        catalogue_path = os.path.join(self.root, check.CATALOGUE)
        with open(catalogue_path) as f:
            catalogue = json.load(f)
        catalogue["entries"].append(
            {"id": "D-99", "class": "dependency", "kind": "feature", "settings": ["x.enabled"], "dependsOn": "Y on", "reasons": []}
        )
        with open(catalogue_path, "w") as f:
            json.dump(catalogue, f)
        problems = check.check(self.root, write_docs=True)
        self.assertIn("D-99: no native test names it (test/)", problems)
        self.assertIn("D-99: no web test names it (web/src)", problems)
        self.assertIn("D-99: a feature dependency needs a `dep: D-99` marker where the device enforces it", problems)

    def test_a_marker_for_an_unknown_entry_fails(self):
        with open(os.path.join(self.root, "src", "Other.cpp"), "w") as f:
            f.write("void f() {} // dep: D-98\n")
        self.assertTrue(any("D-98, which isn't in" in p for p in check.check(self.root)))

    def test_a_constraint_message_the_ui_doesnt_share_fails(self):
        schema = os.path.join(self.root, "web/src/validation/configSchema.ts")
        with open(schema) as f:
            text = f.read()
        with open(schema, "w") as f:
            f.write(text.replace("HTTP auth password is required when auth is enabled", "Password needed"))
        self.assertTrue(any(p.startswith("D-34: message") for p in check.check(self.root)))


if __name__ == "__main__":
    unittest.main()
