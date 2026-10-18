"""Tests for the firmware flash budget and the USB flash package (spec 027)."""

import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _load(name: str):
    """Imports a sibling tool by path (the tools folder isn't a package)."""
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(f"{name}.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


flash_budget = _load("flash_budget")
usb_package = _load("usb_package")
SLOT = 0x1C0000


class FlashBudgetTest(unittest.TestCase):
    def test_slot_from_the_real_layout(self):
        self.assertEqual(flash_budget.app_slot_bytes(ROOT / "partitions.csv"), SLOT)

    def test_each_build_has_its_own_slot(self):
        slots = {env: flash_budget.app_slot_bytes(flash_budget.partitions_for(env)) for env in flash_budget.ENVS}
        self.assertEqual(slots["esp32dev"], SLOT)
        self.assertEqual(slots["esp32dev-ble"], SLOT)
        self.assertEqual(slots["esp32s3-zero"], SLOT)
        self.assertEqual(slots["esp32s3-zero-ble"], SLOT)
        self.assertEqual(slots["esp32s3-zero8"], 0x380000)
        self.assertEqual(slots["esp32s3-zero8-ble"], 0x380000)
        self.assertEqual(slots["esp32s3-eth"], 0x600000)
        self.assertEqual(slots["esp32s3-eth-ble"], 0x600000)

    def test_every_build_is_recorded(self):
        record = json.loads((ROOT / flash_budget.BASELINE_PATH).read_text())
        self.assertEqual([env for env in record if env != flash_budget.REASON_KEY], list(flash_budget.ENVS))

    def test_layouts_keep_settings_offsets(self):
        # nvs and otadata stay put on every layout, so a USB flash keeps the settings.
        for env in flash_budget.ENVS:
            rows = [
                [f.strip() for f in line.split(",")]
                for line in flash_budget.partitions_for(env).read_text().splitlines()
                if line.strip() and not line.lstrip().startswith("#")
            ]
            by_name = {row[0]: (int(row[3], 0), int(row[4], 0)) for row in rows}
            self.assertEqual(by_name["nvs"], (0x9000, 0x5000), env)
            self.assertEqual(by_name["otadata"], (0xE000, 0x2000), env)
            self.assertEqual(by_name["app0"][0], 0x10000, env)

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

    def test_s3_package(self):
        csv = ROOT / "partitions_16MB.csv"
        offsets = {part["path"]: part["offset"] for part in usb_package.parts(csv, "esp32s3")}
        self.assertEqual(offsets["bootloader.bin"], 0x0)  # S3 bootloader at 0
        self.assertEqual(offsets["littlefs.bin"], 0xC10000)
        for offset in offsets.values():
            self.assertFalse(0x9000 <= offset < 0xE000)
        manifest = usb_package.manifest("standard", "0.4.0-alpha.1", csv, "esp32s3", "esp32s3-eth")
        self.assertEqual(manifest["builds"][0]["chipFamily"], "ESP32-S3")
        self.assertIn("esp32s3-eth", manifest["name"])
        self.assertIn("--chip esp32s3", usb_package.esptool_command(csv, "esp32s3"))


if __name__ == "__main__":
    unittest.main()
