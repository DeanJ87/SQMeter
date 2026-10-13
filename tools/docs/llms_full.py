"""MkDocs hook: write site/llms-full.txt, every docs page's Markdown in nav order.

docs/llms.txt is the short, hand-written summary for language models (the
llms.txt convention); llms-full.txt is the whole documentation as one text file
for tools that want it all. Built from the Markdown MkDocs already read, so it
can't drift from the site. No network, standard library only.

Registered in mkdocs.yml under `hooks:`.
"""

from __future__ import annotations

import re
from pathlib import Path

HEADER = (
    "# SQMeter documentation (full text)\n\n"
    "> Every page of https://sqmeter.dev/ as Markdown, in site order. The short summary is "
    "https://sqmeter.dev/llms.txt\n"
)
# Diagram freshness markers and other HTML comments mean nothing outside the repo.
COMMENT = re.compile(r"<!--.*?-->\n?", re.DOTALL)

_pages: dict[str, tuple[str, str]] = {}
_order: list[str] = []


def clean(markdown: str) -> str:
    """The page's Markdown without HTML comments or runs of blank lines."""
    text = COMMENT.sub("", markdown)
    return re.sub(r"\n{3,}", "\n\n", text).strip() + "\n"


def on_pre_build(config, **kwargs) -> None:
    _pages.clear()
    _order.clear()


def on_nav(nav, config, files, **kwargs):
    _order.extend(page.file.src_uri for page in nav.pages)
    return nav


def on_page_markdown(markdown, page, config, files, **kwargs):
    url = config["site_url"].rstrip("/") + "/" + page.url
    _pages[page.file.src_uri] = (url, markdown)
    return markdown


def on_post_build(config, **kwargs) -> None:
    order = _order + sorted(uri for uri in _pages if uri not in _order)
    parts = [HEADER]
    for uri in order:
        if uri in _pages:
            url, markdown = _pages[uri]
            parts.append(f"\n---\n\nSource: {url}\n\n{clean(markdown)}")
    Path(config["site_dir"], "llms-full.txt").write_text("".join(parts), encoding="utf-8")
