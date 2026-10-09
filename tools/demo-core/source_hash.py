#!/usr/bin/env python3
"""Hash of everything the demo's device core is built from. build.sh writes it
to web/src/demo/core/SOURCE_HASH; CI runs this with --check and fails if the
firmware code changed without rebuilding the core (spec 016, FR-010)."""

import glob
import hashlib
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
STAMP = os.path.join(ROOT, "web", "src", "demo", "core", "SOURCE_HASH")


def sources():
    patterns = [
        "tools/demo-core/bridge.cpp",
        "tools/demo-core/build.sh",
        "lib/*/src/*.cpp",
        "lib/*/src/*.h",
        "lib/*/include/*.h",
        "lib/*/include/*/*.h",
    ]
    files = set()
    for pattern in patterns:
        files.update(glob.glob(os.path.join(ROOT, pattern)))
    return sorted(os.path.relpath(f, ROOT) for f in files)


def digest():
    h = hashlib.sha256()
    for rel in sources():
        h.update(rel.encode())
        with open(os.path.join(ROOT, rel), "rb") as f:
            h.update(f.read())
    return h.hexdigest()


if __name__ == "__main__":
    current = digest()
    if "--check" in sys.argv:
        stamped = open(STAMP).read().strip() if os.path.exists(STAMP) else ""
        if stamped != current:
            print("The demo's device core is out of date: lib/ or tools/demo-core changed.")
            print("Rebuild it with tools/demo-core/build.sh (needs Emscripten, see tools/demo-core/VERSION) and commit web/src/demo/core.")
            sys.exit(1)
        print("Demo device core is up to date.")
    else:
        print(current)
