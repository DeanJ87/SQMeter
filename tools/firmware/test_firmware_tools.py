"""Tests for the firmware flash budget and the USB flash package (spec 027)."""

import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import flash_budget  # noqa: E402
import usb_package  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SLOT = 0x1C0000


class FlashBudgetTest(unittest.TestCase):
    def test_slot_from_the_real_layout(self):
        self.assertEqual(flash_budget.app_slot_bytes(ROOT / "partitions.csv"), SLOT)

    def test_under_warning(self):
        failures, warnings, _ = flash_budget.evaluate("esp32dev", int(SLOT * 0.85), SLOT, {}, None)
        self.assertEqual((failures, warnings), ([], []))

    def test_warns_above_90(self):
        failures, warnings, _ = flash_budget.evaluate("esp32dev-ble", int(SLOT * 0.93), SLOT, {}, None)
        self.assertEqual(failures, [])
        self.assertEqual(len(warnings), 1)

    def test_fails_above_95(self):
        failures, _, _ = flash_budget.evaluate("esp32dev-ble", int(SLOT * 0.96), SLOT, {}, None)
        self.assertEqual(len(failures), 1)
        self.assertIn("SIZE-04", failures[0])

    def test_reason_waives_only_the_change_that_adds_it(self):
        size = int(SLOT * 0.96)
        record = {"esp32dev-ble": size, "overBudgetReason": "needed for X"}
        failures, _, _ = flash_budget.evaluate("esp32dev-ble", size, SLOT, record, {"esp32dev-ble": size - 100})
        self.assertEqual(failures, [])
        # The next change inherits the reason from the target branch: no longer waived.
        failures, _, _ = flash_budget.evaluate("esp32dev-ble", size, SLOT, record, dict(record))
        self.assertEqual(len(failures), 1)

    def test_stale_record_fails(self):
        failures, _, _ = flash_budget.evaluate("esp32dev", 1_500_000, SLOT, {"esp32dev": 1_400_000}, None)
        self.assertTrue(any("records" in f for f in failures))

    def test_change_from_target_branch(self):
        _, _, lines = flash_budget.evaluate("esp32dev", 1_500_000, SLOT, {"esp32dev": 1_500_000}, {"esp32dev": 1_498_000})
        self.assertIn("+2,000 B", "\n".join(lines))


class UsbPackageTest(unittest.TestCase):
    def test_parts_skip_nvs_and_match_the_layout(self):
        offsets = {part["path"]: part["offset"] for part in usb_package.parts()}
        self.assertEqual(offsets["bootloader.bin"], 0x1000)
        self.assertEqual(offsets["partitions.bin"], 0x8000)
        self.assertEqual(offsets["boot_app0.bin"], 0xE000)
        self.assertEqual(offsets["firmware.bin"], 0x10000)
        self.assertEqual(offsets["littlefs.bin"], usb_package.fs_offset(ROOT / "partitions.csv"))
        # Nothing is written to NVS (0x9000-0xDFFF): settings are kept.
        for offset in offsets.values():
            self.assertFalse(0x9000 <= offset < 0xE000)

    def test_package(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            sources = {}
            for name in ("bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin", "littlefs.bin"):
                sources[name] = tmp / name
                sources[name].write_bytes(b"\x00" * 16)
            out = tmp / "pkg.zip"
            usb_package.build(sources, "ble", "0.3.0-beta.1", ROOT / "partitions.csv", out)
            with zipfile.ZipFile(out) as z:
                names = set(z.namelist())
                manifest = json.loads(z.read("manifest.json"))
                readme = z.read("FLASH.txt").decode()
            self.assertTrue({"bootloader.bin", "firmware.bin", "littlefs.bin", "manifest.json", "FLASH.txt"} <= names)
            self.assertTrue(manifest["new_install_prompt_erase"])
            self.assertEqual(manifest["builds"][0]["chipFamily"], "ESP32")
            self.assertIn("0x390000 littlefs.bin", readme)
            command = next(line for line in readme.splitlines() if "write_flash" in line)
            self.assertNotIn("erase", command)


if __name__ == "__main__":
    unittest.main()
