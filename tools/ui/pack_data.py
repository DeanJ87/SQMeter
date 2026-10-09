#!/usr/bin/env python3
"""Pack the built web UI (web/dist) into the device's LittleFS data folder.

Text files (html, js, css, svg, json) are stored gzipped as `<name>.gz` with
the original removed: the web server sends `<name>.gz` with
`Content-Encoding: gzip` when `<name>` is asked for (ESPAsyncWebServer's
static handler and AsyncFileResponse both fall back to `.gz`). Fonts (woff2)
are already compressed and are copied as they are. Demo-only files (the MSW
service worker) never go to the device.

Used by scripts/build_web.py (pio buildfs / uploadfs) and by CI.
"""

from __future__ import annotations

import argparse
import gzip
import shutil
import sys
from pathlib import Path

GZIP_SUFFIXES = {".html", ".js", ".css", ".svg", ".json", ".txt", ".ico", ".webmanifest"}
EXCLUDED_NAMES = {"mockServiceWorker.js"}
EXCLUDED_SUFFIXES = {".map"}


def gzip_bytes(data: bytes) -> bytes:
    # mtime=0 keeps the output identical for identical input (reproducible images).
    return gzip.compress(data, compresslevel=9, mtime=0)


def pack(dist: Path, out: Path) -> list[tuple[str, int, int]]:
    """Fill `out` from `dist`; returns (stored path, source bytes, stored bytes) per file."""
    if not dist.is_dir():
        raise FileNotFoundError(f"{dist} not found - build the web UI first")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    stored: list[tuple[str, int, int]] = []
    for src in sorted(p for p in dist.rglob("*") if p.is_file()):
        rel = src.relative_to(dist)
        if src.name in EXCLUDED_NAMES or src.suffix in EXCLUDED_SUFFIXES:
            continue
        data = src.read_bytes()
        dest = out / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if src.suffix in GZIP_SUFFIXES:
            dest = dest.with_name(dest.name + ".gz")
            payload = gzip_bytes(data)
        else:
            payload = data
        dest.write_bytes(payload)
        stored.append((dest.relative_to(out).as_posix(), len(data), len(payload)))
    if not (out / "index.html.gz").exists():
        raise RuntimeError("index.html missing from the web build")
    return stored


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dist", type=Path, default=Path("web/dist"))
    parser.add_argument("--out", type=Path, default=Path("data"))
    args = parser.parse_args(argv)
    stored = pack(args.dist, args.out)
    source = sum(s for _, s, _ in stored)
    packed = sum(p for _, _, p in stored)
    for path, src, out in stored:
        print(f"  {path:<40} {src / 1024:8.1f} KB -> {out / 1024:7.1f} KB")
    print(f"Packed {len(stored)} files: {source / 1024:.1f} KB -> {packed / 1024:.1f} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
