#!/usr/bin/env python3
"""Checks the docs' Mermaid diagrams (spec 024).

Every ```mermaid block in docs/**/*.md and README.md must be complete
(metadata comment, accTitle/accDescr, caption, "Diagram in words", no inline
styling) and fresh: its recorded fingerprint must match a hash of the source
code it says it reflects. A stale diagram is a warning, unless it's marked
`blocking: true` (the safety-verdict diagrams), when it's an error.

    python3 tools/docs/diagrams.py                 # check (exit 1 on errors)
    python3 tools/docs/diagrams.py --confirm DIA-02 # record that DIA-02 matches the code
    python3 tools/docs/diagrams.py --confirm all
    python3 tools/docs/diagrams.py --site site     # built site serves its own mermaid, no CDN

The block format is in specs/024-docs-diagrams/contracts/diagram-block.md.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

FENCE_OPEN = re.compile(r"^(\s*)```mermaid\s*$")
META_START = re.compile(r"^\s*<!--\s*diagram:\s*(\S+)\s*$")
DIAGRAM_ID = re.compile(r"^DIA-\d{2}$")
FINGERPRINT = re.compile(r"^[0-9a-f]{16}$")
INLINE_STYLE = re.compile(r"^\s*(classDef|style|linkStyle)\s|%%\{")
CAPTION = re.compile(r"<figcaption>\s*\S.*</figcaption>|^\s*\*Figure:\s*\S.*\*\s*$")
WORDS = re.compile(r'^\s*\?\?\?\+?\s+\w+\s+"Diagram in words"\s*$|<summary>\s*Diagram in words\s*</summary>')
CDN_HOSTS = ("unpkg.com", "cdn.jsdelivr.net")
VENDORED = "assets/javascripts/vendor/mermaid.min.js"

# How far after a diagram the caption and "in words" block may start.
LOOKAHEAD_LINES = 8


@dataclass
class Diagram:
    id: str
    file: Path
    line: int  # 1-based line of the ```mermaid fence
    body: str
    sources: list[str] = field(default_factory=list)
    blocking: bool = False
    fingerprint: str | None = None
    meta_line: int | None = None  # 1-based line of "<!-- diagram:"
    problems: list[str] = field(default_factory=list)

    @property
    def where(self) -> str:
        return f"{self.file.relative_to(ROOT)}:{self.line}"


class SourceError(Exception):
    pass


# --- Sources and fingerprints ------------------------------------------------


def _matching(text: str, start: int, open_char: str, close_char: str) -> int:
    """Index of the bracket closing the one at `start`, or -1."""
    depth = 0
    for k in range(start, len(text)):
        if text[k] == open_char:
            depth += 1
        elif text[k] == close_char:
            depth -= 1
            if depth == 0:
                return k
    return -1


def _definition_span(text: str, symbol: str) -> str:
    """The definition of `symbol` in C/C++-like source: from the line where it
    is defined (a `symbol(` whose parameter list is followed by `{`) to the
    matching closing brace. Declarations and calls are skipped."""
    for match in re.finditer(re.escape(symbol) + r"\s*\(", text):
        start = match.start()
        if start > 0 and (text[start - 1].isalnum() or text[start - 1] == "_"):
            continue  # part of a longer name
        params_end = _matching(text, match.end() - 1, "(", ")")
        if params_end < 0:
            continue
        after = re.match(r"\s*(?:const\s*|override\s*|noexcept\s*|->\s*[\w:<>]+\s*)*", text[params_end + 1 :])
        body_start = params_end + 1 + after.end()
        if body_start >= len(text) or text[body_start] != "{":
            continue  # a declaration or a call
        body_end = _matching(text, body_start, "{", "}")
        if body_end < 0:
            raise SourceError(f"unbalanced braces after {symbol}")
        line_start = text.rfind("\n", 0, start) + 1
        return text[line_start : body_end + 1]
    raise SourceError(f"no definition of {symbol}")


def _directory_files(path: Path, root: Path) -> list[Path]:
    """Files under `path`, sorted. In a git checkout only tracked files count,
    so local build output never changes a fingerprint; elsewhere (tests)
    every file except hidden ones below `path`."""
    try:
        listed = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z", "--", path.relative_to(root).as_posix()],
            capture_output=True,
            check=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        listed = None
    if listed is not None and (root / ".git").exists():
        return sorted(root / name for name in listed.decode().split("\0") if name)
    return sorted(p for p in path.rglob("*") if p.is_file() and not any(part.startswith(".") for part in p.relative_to(path).parts))


def source_content(ref: str, root: Path = ROOT) -> str:
    path_part, _, symbol = ref.partition("#")
    path = root / path_part
    if not path.exists():
        raise SourceError(f"{path_part} doesn't exist")
    if path.is_dir():
        files = _directory_files(path, root)
        if not files:
            raise SourceError(f"{path_part} has no files")
        return "".join(f"{p.relative_to(root).as_posix()}\n{p.read_text(encoding='utf-8', errors='replace')}\n" for p in files)
    text = path.read_text(encoding="utf-8", errors="replace")
    if symbol:
        try:
            return _definition_span(text, symbol)
        except SourceError as error:
            raise SourceError(f"{path_part}: {error}") from None
    return text


def fingerprint(sources: list[str], root: Path = ROOT) -> str:
    digest = hashlib.sha256()
    for ref in sources:
        digest.update(f"{ref}\n{source_content(ref, root)}\n".encode())
    return digest.hexdigest()[:16]


# --- Parsing -----------------------------------------------------------------


def _parse_meta(lines: list[str], fence_index: int) -> tuple[dict[str, str], int | None]:
    """Metadata comment above the fence (blank lines and <figure> allowed
    between). Returns (fields, 0-based index of the "<!-- diagram:" line)."""
    i = fence_index - 1
    while i >= 0 and (not lines[i].strip() or lines[i].strip().startswith("<figure")):
        i -= 1
    if i < 0 or not lines[i].strip().endswith("-->"):
        return {}, None
    end = i
    while i >= 0 and not META_START.match(lines[i]):
        if "<!--" in lines[i] and i != end:
            return {}, None
        i -= 1
    if i < 0:
        return {}, None
    fields = {"diagram": META_START.match(lines[i]).group(1)}
    for raw in lines[i + 1 : end + 1]:
        text = raw.strip().removesuffix("-->").strip()
        if not text:
            continue
        key, sep, value = text.partition(":")
        if sep:
            fields[key.strip()] = value.strip()
    return fields, i


def _check_meta(diagram: Diagram, meta: dict[str, str]) -> None:
    if not meta:
        diagram.problems.append("no <!-- diagram: DIA-NN ... --> metadata comment above it")
        return
    if not DIAGRAM_ID.match(diagram.id):
        diagram.problems.append(f"bad diagram id {diagram.id!r} (want DIA-NN)")
    diagram.sources = meta.get("sources", "").split()
    if not diagram.sources:
        diagram.problems.append("no sources: listed")
    blocking = meta.get("blocking", "false").lower()
    if blocking not in ("true", "false"):
        diagram.problems.append(f"blocking must be true or false, not {blocking!r}")
    diagram.blocking = blocking == "true"
    diagram.fingerprint = meta.get("fingerprint")


def _check_body(diagram: Diagram) -> None:
    if "accTitle:" not in diagram.body:
        diagram.problems.append("no accTitle: (screen reader title)")
    if not re.search(r"accDescr\s*[:{]", diagram.body):
        diagram.problems.append("no accDescr (screen reader description)")
    for number, line in enumerate(diagram.body.splitlines(), start=diagram.line + 1):
        if INLINE_STYLE.search(line):
            diagram.problems.append(f"inline styling on line {number} - diagrams take the site's colours (FR-003)")


def _check_text_after(diagram: Diagram, after: list[str]) -> None:
    """The caption, then a non-empty "Diagram in words" block, just below."""
    caption_at = next((k for k, line in enumerate(after[:LOOKAHEAD_LINES]) if CAPTION.search(line)), None)
    if caption_at is None:
        diagram.problems.append("no caption (<figcaption>...</figcaption> or *Figure: ...*) right after it")
        return
    rest = after[caption_at + 1 : caption_at + 1 + LOOKAHEAD_LINES]
    words_at = next((k for k, line in enumerate(rest) if WORDS.search(line)), None)
    if words_at is None:
        diagram.problems.append('no "Diagram in words" block after the caption')
    elif not [line for line in rest[words_at + 1 :] if line.strip() and "</details>" not in line]:
        diagram.problems.append('the "Diagram in words" block is empty')


def parse_file(path: Path) -> list[Diagram]:
    lines = path.read_text(encoding="utf-8").splitlines()
    diagrams = []
    i = 0
    while i < len(lines):
        opened = FENCE_OPEN.match(lines[i])
        if not opened:
            i += 1
            continue
        indent = opened.group(1)
        j = i + 1
        while j < len(lines) and lines[j].strip() != "```":
            j += 1
        body = "\n".join(line[len(indent) :] if line.startswith(indent) else line for line in lines[i + 1 : j])
        meta, meta_index = _parse_meta(lines, i)
        diagram = Diagram(id=meta.get("diagram", "?"), file=path, line=i + 1, body=body)
        diagram.meta_line = meta_index + 1 if meta_index is not None else None
        if j >= len(lines):
            diagram.problems.append("unclosed ```mermaid fence")
        _check_meta(diagram, meta)
        _check_body(diagram)
        _check_text_after(diagram, lines[j + 1 : j + 1 + 2 * LOOKAHEAD_LINES])
        diagrams.append(diagram)
        i = j + 1
    return diagrams


def markdown_files(root: Path = ROOT) -> list[Path]:
    files = sorted((root / "docs").rglob("*.md"))
    readme = root / "README.md"
    return files + ([readme] if readme.exists() else [])


def collect(root: Path = ROOT) -> list[Diagram]:
    return [diagram for path in markdown_files(root) for diagram in parse_file(path)]


# --- Checks ------------------------------------------------------------------


@dataclass
class Report:
    errors: list[tuple[Diagram | None, str]] = field(default_factory=list)
    warnings: list[tuple[Diagram | None, str]] = field(default_factory=list)
    lines: list[str] = field(default_factory=list)


def _freshness(diagram: Diagram, report: Report, root: Path) -> str:
    try:
        current = fingerprint(diagram.sources, root)
    except SourceError as error:
        report.errors.append((diagram, f"bad source: {error}"))
        return "bad-source"
    if diagram.fingerprint == current:
        return "ok"
    message = (
        f"stale: its sources changed (recorded {diagram.fingerprint or 'none'}, now {current}). Check the diagram against "
        f"{' '.join(diagram.sources)}, update it if needed, then run "
        f"`python3 tools/docs/diagrams.py --confirm {diagram.id}`"
    )
    if diagram.blocking:
        report.errors.append((diagram, message + " - this is a safety diagram, so it blocks"))
        return "stale-blocking"
    report.warnings.append((diagram, message))
    return "stale"


def check(diagrams: list[Diagram], root: Path = ROOT) -> Report:
    report = Report()
    by_id: dict[str, list[Diagram]] = {}
    for diagram in diagrams:
        by_id.setdefault(diagram.id, []).append(diagram)
        report.errors.extend((diagram, problem) for problem in diagram.problems)
        if diagram.problems:
            state = "incomplete"
        elif diagram.sources:
            state = _freshness(diagram, report, root)
        else:
            state = "ok"
        report.lines.append(f"{state:<15}{diagram.id:<8}{diagram.where}")
    for diagram_id, copies in by_id.items():
        if len({copy.body.strip() for copy in copies}) > 1:
            report.errors.append((copies[0], f"{diagram_id} appears {len(copies)} times with different content (FR-012)"))
            report.lines.append(f"{'mismatch':<15}{diagram_id:<8}{', '.join(copy.where for copy in copies)}")
    return report


def check_site(site: Path) -> list[str]:
    problems = []
    if not (site / VENDORED).is_file():
        problems.append(f"vendored mermaid missing: {site / VENDORED} (run `npm --prefix web run docs:vendor` before mkdocs build)")
    pages = 0
    for page in sorted(site.rglob("*.html")):
        html = page.read_text(encoding="utf-8", errors="replace")
        for host in CDN_HOSTS:
            if host in html:
                problems.append(f"{page.relative_to(site)} references {host} (FR-004: no third-party hosts)")
        if 'class="mermaid"' in html:
            pages += 1
            if "vendor/mermaid.min.js" not in html:
                problems.append(f"{page.relative_to(site)} has a diagram but doesn't load the vendored mermaid")
    if not problems:
        print(f"site: {pages} diagram pages, all load the vendored mermaid; no CDN references")
    return problems


def confirm(diagrams: list[Diagram], ids: list[str], root: Path = ROOT) -> int:
    wanted = {diagram.id for diagram in diagrams} if ids == ["all"] else set(ids)
    unknown = wanted - {diagram.id for diagram in diagrams}
    if unknown:
        print(f"unknown diagram(s): {', '.join(sorted(unknown))}", file=sys.stderr)
        return 1
    edits: dict[Path, list[Diagram]] = {}
    for diagram in diagrams:
        if diagram.id in wanted:
            if diagram.meta_line is None or not diagram.sources:
                print(f"{diagram.where}: {diagram.id} has no metadata/sources to confirm", file=sys.stderr)
                return 1
            edits.setdefault(diagram.file, []).append(diagram)
    for path, items in edits.items():
        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        for diagram in sorted(items, key=lambda d: d.meta_line, reverse=True):
            value = fingerprint(diagram.sources, root)
            start = diagram.meta_line - 1
            end = start
            while not lines[end].rstrip().endswith("-->"):
                end += 1
            for k in range(start + 1, end + 1):
                if lines[k].strip().startswith("fingerprint:"):
                    closes = lines[k].rstrip().endswith("-->")
                    lines[k] = f"fingerprint: {value}{' -->' if closes else ''}\n"
                    break
            else:
                lines.insert(end, f"fingerprint: {value}\n")
            print(f"confirmed      {diagram.id:<8}{diagram.where}  {value}")
        path.write_text("".join(lines), encoding="utf-8")
    return 0


def _annotate(kind: str, diagram: Diagram | None, message: str) -> str:
    if diagram is None:
        return f"{kind}: {message}"
    if os.environ.get("GITHUB_ACTIONS"):
        print(f"::{kind} file={diagram.file.relative_to(ROOT)},line={diagram.line},title={diagram.id}::{message}")
    return f"{kind}: {diagram.where} {diagram.id}: {message}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--confirm", nargs="+", metavar="ID", help="record current fingerprints (DIA-NN ... or all)")
    parser.add_argument("--site", type=Path, help="check a built site directory instead")
    args = parser.parse_args(argv)

    if args.site:
        problems = check_site(args.site)
        for problem in problems:
            print(f"error: {problem}")
        return 1 if problems else 0

    diagrams = collect()
    if args.confirm:
        return confirm(diagrams, args.confirm)

    report = check(diagrams)
    print("\n".join(report.lines))
    for diagram, message in report.warnings:
        print(_annotate("warning", diagram, message))
    for diagram, message in report.errors:
        print(_annotate("error", diagram, message))
    print(f"{len(diagrams)} diagrams, {len(report.errors)} errors, {len(report.warnings)} warnings")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
