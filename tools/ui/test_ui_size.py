"""Tests for the device UI packer and size budgets (SIZE-01, SIZE-02)."""

import gzip
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(f"{name}.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pack_data = load("pack_data")
size_check = load("size_check")


def write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


class PackTests(unittest.TestCase):
    def test_text_gzipped_fonts_copied_demo_files_dropped(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist, out = Path(tmp, "dist"), Path(tmp, "data")
            write(dist / "index.html", b"<html>" * 100)
            write(dist / "assets" / "App.js", b"console.log(1);" * 500)
            write(dist / "assets" / "a.css", b"body{}" * 200)
            write(dist / "assets" / "f.woff2", b"\x00font")
            write(dist / "mockServiceWorker.js", b"msw")
            write(dist / "assets" / "App.js.map", b"{}")
            pack_data.pack(dist, out)
            names = sorted(p.relative_to(out).as_posix() for p in out.rglob("*") if p.is_file())
            self.assertEqual(names, ["assets/App.js.gz", "assets/a.css.gz", "assets/f.woff2", "index.html.gz"])
            self.assertEqual(gzip.decompress((out / "index.html.gz").read_bytes()), b"<html>" * 100)

    def test_packing_is_reproducible(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = Path(tmp, "dist")
            write(dist / "index.html", b"<html></html>")
            pack_data.pack(dist, Path(tmp, "a"))
            pack_data.pack(dist, Path(tmp, "b"))
            self.assertEqual(Path(tmp, "a/index.html.gz").read_bytes(), Path(tmp, "b/index.html.gz").read_bytes())

    def test_missing_index_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            write(Path(tmp, "dist/assets/x.js"), b"1")
            with self.assertRaises(RuntimeError):
                pack_data.pack(Path(tmp, "dist"), Path(tmp, "data"))


class SizeTests(unittest.TestCase):
    def measured(self, js_bytes: int, files: int = 1) -> dict:
        return {
            "files": {f"f{i}": 0 for i in range(files)},
            "js": js_bytes,
            "css": 0,
            "fonts": 0,
            "stored": js_bytes,
            "ui_blocks": sum(size_check.blocks_for(js_bytes // files) for _ in range(files)),
        }

    def test_blocks_include_metadata(self):
        self.assertEqual(size_check.blocks_for(0), 2)
        self.assertEqual(size_check.blocks_for(4096), 2)
        self.assertEqual(size_check.blocks_for(4097), 3)

    def test_partition_size_read_from_table(self):
        with tempfile.TemporaryDirectory() as tmp:
            csv = Path(tmp, "p.csv")
            csv.write_text("# c\nnvs, data, nvs, 0x9000, 0x5000,\nspiffs, data, spiffs, 0x310000,0x80000,\n")
            self.assertEqual(size_check.partition_bytes(csv), 512 * 1024)

    def test_small_ui_passes(self):
        failures, _ = size_check.evaluate(self.measured(150 * 1024, 6), {"p": 512 * 1024}, None)
        self.assertEqual(failures, [])

    def test_full_partition_fails(self):
        failures, _ = size_check.evaluate(self.measured(400 * 1024, 6), {"p": 512 * 1024}, None)
        self.assertTrue(any(f.startswith("SIZE-02") for f in failures))

    def test_smallest_partition_is_the_limit(self):
        failures, _ = size_check.evaluate(self.measured(200 * 1024, 4), {"big": 4 << 20, "small": 256 * 1024}, None)
        self.assertTrue(any("SIZE-02" in f for f in failures))

    def test_growth_over_baseline(self):
        base = {"jsCssGzipBytes": 100 * 1024}
        ok, _ = size_check.evaluate(self.measured(109 * 1024), {"p": 4 << 20}, base)
        self.assertEqual(ok, [])
        bad, _ = size_check.evaluate(self.measured(111 * 1024), {"p": 4 << 20}, base)
        self.assertTrue(any(f.startswith("SIZE-01") for f in bad))

    def test_cli_end_to_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist, data = Path(tmp, "dist"), Path(tmp, "data")
            write(dist / "index.html", b"<html></html>")
            write(dist / "assets/App.js", b"x" * 5000)
            pack_data.pack(dist, data)
            csv = Path(tmp, "p.csv")
            csv.write_text("spiffs, data, spiffs, 0x310000,0x80000,\n")
            baseline = Path(tmp, "b.json")
            argv = ["--data", str(data), "--partitions", str(csv), "--baseline", str(baseline)]
            self.assertEqual(size_check.main(argv + ["--update-baseline"]), 0)
            self.assertIn("jsCssGzipBytes", json.loads(baseline.read_text()))
            self.assertEqual(size_check.main(argv), 0)


if __name__ == "__main__":
    unittest.main()
