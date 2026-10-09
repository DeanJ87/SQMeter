#!/usr/bin/env python3
"""Builds the language files a release publishes (specs/023-i18n FR-008, FR-018).

For each language in web/src/i18n/locales:
  sqmeter-i18n-<code>.json.gz         gzip of {"lang", "version", "messages"}
  sqmeter-i18n-<code>.json.gz.sha256  "<sha256 hex> <size>", read by the device
plus sqmeter-i18n-manifest.json listing every file.

  python3 tools/i18n/build_packs.py --version 0.2.1 --out dist/i18n
"""

import argparse
import gzip
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOCALES = ROOT / "web/src/i18n/locales"
MAX_FILE_BYTES = 64 * 1024  # lib/LanguageLogic MAX_FILE_BYTES


def pack(code: str, version: str, messages: dict) -> bytes:
    body = json.dumps({"lang": code, "version": version, "messages": messages}, ensure_ascii=False, separators=(",", ":"))
    # mtime=0 keeps the output identical for identical input.
    return gzip.compress(body.encode("utf-8"), compresslevel=9, mtime=0)


def build(version: str, out: Path, locales: Path = LOCALES) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    manifest = {"version": version, "languages": []}
    for path in sorted(locales.glob("*.json")):
        code = path.stem
        data = pack(code, version, json.loads(path.read_text(encoding="utf-8")))
        if len(data) > MAX_FILE_BYTES:
            raise SystemExit(f"build_packs.py: {code} is {len(data)} bytes, over the {MAX_FILE_BYTES}-byte limit")
        name = f"sqmeter-i18n-{code}.json.gz"
        digest = hashlib.sha256(data).hexdigest()
        (out / name).write_bytes(data)
        (out / f"{name}.sha256").write_text(f"{digest} {len(data)}\n")
        manifest["languages"].append({"code": code, "file": name, "size": len(data), "sha256": digest})
    (out / "sqmeter-i18n-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", required=True, help="firmware version without the v, e.g. 0.2.1")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    manifest = build(args.version.removeprefix("v"), args.out)
    for entry in manifest["languages"]:
        print(f'{entry["file"]}: {entry["size"]} bytes')
    return 0


if __name__ == "__main__":
    sys.exit(main())
