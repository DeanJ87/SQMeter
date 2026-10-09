"""Tests for tools/docs/flash_assets.py (spec 027 FR-019)."""

import io
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import flash_assets  # noqa: E402


def asset(name):
    return {"name": name, "browser_download_url": f"https://example.com/{name}"}


class FlashAssetsTest(unittest.TestCase):
    def test_newest_release_with_both_packages(self):
        releases = [
            {"tag_name": "v0.3.1", "draft": True, "assets": [asset("sqmeter-l2-usb-standard-v0.3.1.zip"), asset("sqmeter-l2-usb-ble-v0.3.1.zip")]},
            {"tag_name": "v0.3.0-beta.2", "assets": [asset("sqmeter-l2-usb-standard-v0.3.0-beta.2.zip")]},
            {"tag_name": "v0.3.0-beta.1", "assets": [asset("sqmeter-l2-usb-standard-v0.3.0-beta.1.zip"), asset("sqmeter-l2-usb-ble-v0.3.0-beta.1.zip")]},
            {"tag_name": "v0.2.0-beta.3", "assets": [asset("sqmeter-complete-flash-v0.2.0-beta.3.bin")]},
        ]
        tag, urls = flash_assets.pick(releases)
        self.assertEqual(tag, "v0.3.0-beta.1")
        self.assertTrue(urls["ble"].endswith("sqmeter-l2-usb-ble-v0.3.0-beta.1.zip"))

    def test_none_yet(self):
        self.assertIsNone(flash_assets.pick([{"tag_name": "v0.2.0-beta.3", "assets": []}]))

    def test_unpack_refuses_paths(self):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as z:
            z.writestr("../evil.bin", b"x")
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(ValueError):
            flash_assets.unpack(buffer.getvalue(), Path(tmp))


if __name__ == "__main__":
    unittest.main()
