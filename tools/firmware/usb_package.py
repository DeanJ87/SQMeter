#!/usr/bin/env python3
"""One-time USB flash package for the whole-chip layout (spec 027 FR-018, FR-019).

Builds sqmeter-l2-usb-<build>-<tag>.zip: the bootloader, the partition table,
boot_app0 (otadata), the firmware and the web UI image, each at its own
offset, plus a manifest for the browser flasher (ESP Web Tools) and the
equivalent esptool command. NVS (0x9000-0xDFFF) is never written, so a device
keeps its settings - WiFi included - across the move to the new layout.

Usage: usb_package.py --build ble --version 0.3.0-beta.1 --build-dir .pio/build/esp32dev-ble
       --fs .pio/build/esp32dev/littlefs.bin --boot-app0 <path> --out sqmeter-l2-usb-ble-v0.3.0-beta.1.zip
"""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fs_offset(csv: Path) -> int:
    for line in csv.read_text().splitlines():
        if line.lstrip().startswith("#"):
            continue
        fields = [f.strip() for f in line.split(",")]
        if len(fields) >= 5 and fields[2] == "spiffs":
            return int(fields[3], 0)
    raise ValueError(f"no LittleFS partition in {csv}")


def parts(csv: Path = ROOT / "partitions.csv") -> list[dict]:
    return [
        {"path": "bootloader.bin", "offset": 0x1000},
        {"path": "partitions.bin", "offset": 0x8000},
        {"path": "boot_app0.bin", "offset": 0xE000},
        {"path": "firmware.bin", "offset": 0x10000},
        {"path": "littlefs.bin", "offset": fs_offset(csv)},
    ]


def manifest(build: str, version: str, csv: Path) -> dict:
    return {
        "name": f"SQMeter ({'Bluetooth' if build == 'ble' else 'standard'} build)",
        "version": version,
        # Ask, rather than erase by default: leaving "Erase device" unticked
        # keeps NVS and so every setting.
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32", "parts": parts(csv)}],
    }


def esptool_command(csv: Path) -> str:
    writes = " ".join(f"0x{p['offset']:X} {p['path']}" for p in parts(csv))
    return f"esptool.py --chip esp32 --baud 460800 write_flash {writes}"


def readme(build: str, version: str, csv: Path) -> str:
    return (
        f"SQMeter {version} ({build} build) - one-time USB flash to the whole-chip layout.\n\n"
        "Settings (WiFi included) are kept: nothing is written to 0x9000-0xDFFF, where\n"
        "they are stored. Do not tick \"Erase device\" in the browser flasher, and never run\n"
        "erase_flash.\n\n"
        "From this folder, with the device on USB:\n\n"
        f"  {esptool_command(csv)}\n\n"
        "Guide: https://sqmeter.dev/getting-started/usb-flash/\n"
    )


def build(sources: dict[str, Path], build_name: str, version: str, csv: Path, out: Path) -> None:
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for part in parts(csv):
            z.write(sources[part["path"]], part["path"])
        z.writestr("manifest.json", json.dumps(manifest(build_name, version, csv), indent=2) + "\n")
        z.writestr("FLASH.txt", readme(build_name, version, csv))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="One-time USB flash package (spec 027)")
    parser.add_argument("--build", choices=["standard", "ble"], required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--build-dir", type=Path, required=True, help="the env's .pio/build/<env> folder")
    parser.add_argument("--fs", type=Path, required=True, help="littlefs.bin")
    parser.add_argument("--boot-app0", type=Path, required=True)
    parser.add_argument("--partitions", type=Path, default=ROOT / "partitions.csv")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    sources = {
        "bootloader.bin": args.build_dir / "bootloader.bin",
        "partitions.bin": args.build_dir / "partitions.bin",
        "boot_app0.bin": args.boot_app0,
        "firmware.bin": args.build_dir / "firmware.bin",
        "littlefs.bin": args.fs,
    }
    missing = [str(p) for p in sources.values() if not p.exists()]
    if missing:
        print(f"missing: {', '.join(missing)}", file=sys.stderr)
        return 2
    build(sources, args.build, args.version, args.partitions, args.out)
    print(f"Wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
