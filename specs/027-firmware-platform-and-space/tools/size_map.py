#!/usr/bin/env python3
"""Attribute ESP32 firmware flash and static RAM to components, from a GNU ld map.

Research tool for spec 027. Usage: size_map.py firmware.map [--top N] [--json]

Flash = .flash.text + .flash.rodata + .flash.appdesc + .iram0.text/.vectors +
.dram0.data (initialised data is stored in flash).
Static RAM = .dram0.data + .dram0.bss + .iram0.text/.vectors.
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_LIB_DIR = Path(__file__).resolve().parents[3] / 'lib'

FLASH = ('.flash.text', '.flash.rodata', '.flash.appdesc', '.iram0.text', '.iram0.vectors', '.dram0.data')
RAM = ('.dram0.data', '.dram0.bss', '.iram0.text', '.iram0.vectors')
ENTRY = re.compile(r'^\s+(?:\S+\s+)?0x([0-9a-f]{8,16})\s+0x([0-9a-f]+)\s+(\S.*)$')
SECTION = re.compile(r'^(\.[\w.]+)\s+0x[0-9a-f]+\s+0x[0-9a-f]+')
OWN_LIB = re.compile(r'/build/[^/]+/lib[0-9a-f]+/(\w+)/')
ARCHIVE = re.compile(r'/lib([\w+-]+)\.a\(')


def component(path: str) -> str:
    p = path.strip()
    if '/libdeps/' in p:
        return 'lib:' + p.split('/libdeps/')[1].split('/')[1]
    archive = ARCHIVE.search(p)
    if ('framework-arduinoespressif32' in p or 'framework-arduinoespressif32-libs' in p) and archive:
        return 'idf:' + archive.group(1)
    if 'framework-arduinoespressif32' in p or '/FrameworkArduino/' in p:
        return 'arduino-core'
    if 'toolchain' in p and archive:
        return 'toolchain:' + archive.group(1)
    if '/build/' in p and '/src/' in p:
        return 'ours:src/' + p.split('/src/')[1].split('.cpp')[0].split('/')[0]
    own = OWN_LIB.search(p)
    if own:
        return 'ours:lib/' + own.group(1)
    if archive and '.pio/build/' in p:
        name = archive.group(1)
        # Project libraries (lib/<Name>) vs Arduino/third-party libraries built
        # from source by PlatformIO (WiFi, HTTPClient, ESPAsyncWebServer...).
        if (PROJECT_LIB_DIR / name).is_dir():
            return 'ours:lib/' + name
        return 'lib:' + name
    if archive:
        return 'other:' + archive.group(1)
    return 'other:' + p.split('/')[-1][:40]


def parse(path):
    sizes = {s: defaultdict(int) for s in set(FLASH) | set(RAM)}
    current = None
    for line in open(path, errors='replace'):
        if line.startswith('.') or line.startswith('/DISCARD/'):
            sm = SECTION.match(line)
            name = line.split()[0]
            current = name if name in sizes else None
            continue
        if current is None:
            continue
        m = ENTRY.match(line)
        if not m:
            continue
        size = int(m.group(2), 16)
        target = m.group(3)
        if size and ('.o' in target or '.a(' in target or 'obj' in target):
            sizes[current][component(target)] += size
    return sizes


def totals(sizes, names):
    out = defaultdict(int)
    for s in names:
        for k, v in sizes.get(s, {}).items():
            out[k] += v
    return out


def main():
    args = sys.argv[1:]
    top = int(args[args.index('--top') + 1]) if '--top' in args else 30
    sizes = parse(args[0])
    flash = totals(sizes, FLASH)
    ram = totals(sizes, RAM)
    groups = defaultdict(int)
    for k, v in flash.items():
        groups[k.split(':')[0]] += v
    if '--json' in args:
        print(json.dumps({'flash': flash, 'ram': ram, 'groups': groups}, indent=1))
        return
    total = sum(flash.values())
    print(f'Flash attributed: {total:,} B')
    for g, v in sorted(groups.items(), key=lambda x: -x[1]):
        print(f'  {g:12} {v:>10,} B {100 * v / total:5.1f}%')
    print(f'\nTop {top} components by flash:')
    rodata = sizes['.flash.rodata']
    for k, v in sorted(flash.items(), key=lambda x: -x[1])[:top]:
        print(f'  {k:44} {v:>9,} B  (rodata {rodata.get(k, 0):,})')
    print('\nTop 15 by static RAM:')
    for k, v in sorted(ram.items(), key=lambda x: -x[1])[:15]:
        print(f'  {k:44} {v:>9,} B')


if __name__ == '__main__':
    main()
