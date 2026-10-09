#!/usr/bin/env python3
"""Puts the newest release's USB flash packages on the docs site (spec 027 FR-019).

The browser flasher (docs/getting-started/usb-flash.md, ESP Web Tools) reads
site/flash/<standard|ble>/manifest.json and the parts next to it, from
sqmeter.dev itself: GitHub release downloads can't be fetched by a page.
Finds the newest release (pre-releases included) that has
sqmeter-l2-usb-standard-*.zip and sqmeter-l2-usb-ble-*.zip and unpacks them.
With no such release yet, writes nothing and the page says so.

  python3 tools/docs/flash_assets.py --site site [--repo DeanJ87/SQMeter]
Set GH_TOKEN to avoid the API's anonymous rate limit. Standard library only.
"""

from __future__ import annotations

import argparse
import io
import json
import os
import sys
import urllib.request
import zipfile
from pathlib import Path

BUILDS = ("standard", "ble")
PREFIX = "sqmeter-l2-usb-"


def api(url: str) -> object:
    request = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
    token = os.environ.get("GH_TOKEN")
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def pick(releases: list[dict]) -> tuple[str, dict[str, str]] | None:
    """(tag, {build: zip download URL}) of the newest release with both packages."""
    for release in releases:
        if release.get("draft"):
            continue
        urls = {}
        for asset in release.get("assets", []):
            name = asset.get("name", "")
            for build in BUILDS:
                if name.startswith(f"{PREFIX}{build}-") and name.endswith(".zip"):
                    urls[build] = asset["browser_download_url"]
        if all(build in urls for build in BUILDS):
            return release["tag_name"], urls
    return None


def unpack(data: bytes, target: Path) -> None:
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for name in z.namelist():
            if "/" in name or name.startswith("."):
                raise ValueError(f"unexpected entry {name!r} in the package")
            (target / name).write_bytes(z.read(name))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="USB flash packages for the docs site")
    parser.add_argument("--site", type=Path, required=True)
    parser.add_argument("--repo", default="DeanJ87/SQMeter")
    args = parser.parse_args(argv)
    found = pick(api(f"https://api.github.com/repos/{args.repo}/releases?per_page=20"))  # type: ignore[arg-type]
    if found is None:
        print("No release has USB flash packages yet; the flasher page says so.")
        return 0
    tag, urls = found
    for build, url in urls.items():
        with urllib.request.urlopen(url, timeout=120) as response:
            unpack(response.read(), args.site / "flash" / build)
    (args.site / "flash" / "release.json").write_text(json.dumps({"tag": tag}) + "\n")
    print(f"Flasher files from {tag} -> {args.site / 'flash'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
