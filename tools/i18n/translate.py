#!/usr/bin/env python3
"""Translates new and changed UI strings with the Anthropic API (specs/023-i18n FR-021, D12).

For each language it sends only the keys whose English changed since they were
last translated (tools/i18n/record.json), with each key's context note, the
language's glossary and register. A second call reviews the draft as a native
UI editor would (fluency, glossary, length, placeholders) and back-translates
the safety strings. The result is checked with tools/i18n/check.mjs and only
written when it passes; the review notes are appended to tools/i18n/review/<code>.md.

  ANTHROPIC_API_KEY=... python3 tools/i18n/translate.py --lang es    one language
  python3 tools/i18n/translate.py --all                               every language
  python3 tools/i18n/translate.py --all --dry-run                     list what would change
  python3 tools/i18n/translate.py --all --record                      mark the current files as up to date

Standard library only.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
I18N = ROOT / "web/src/i18n"
TOOLS = ROOT / "tools/i18n"
RECORD = TOOLS / "record.json"
CHECK = Path(__file__).resolve().parent / "check.mjs"
API_URL = "https://api.anthropic.com/v1/messages"
MODEL = "claude-sonnet-5"
BATCH = 150
# Strings whose meaning must survive exactly; the reviewer back-translates them.
SAFETY_KEYS = [
    "safetyCard.safe",
    "safetyCard.unsafe",
    "device.alert.observatoryUnsafe",
    "device.safety.rainDetected",
    "safetyCard.alertsPaused",
]


def read_json(path: Path, default=None):
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else default


def write_json(path: Path, data) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def fingerprint(english) -> str:
    """Hash of the English a translation was made from."""
    return hashlib.sha1(json.dumps(english, ensure_ascii=False, sort_keys=True).encode("utf-8")).hexdigest()[:10]


def languages() -> list[str]:
    text = (I18N / "languages.ts").read_text(encoding="utf-8")
    return [code for code in re.findall(r"code: '([^']+)'", text) if code != "en"]


def changed_keys(en: dict, messages: dict, record: dict) -> list[str]:
    """Keys that are missing or whose English changed since they were translated."""
    return [key for key, english in en.items() if key not in messages or record.get(key) != fingerprint(english)]


def plural_forms(code: str) -> str:
    """The CLDR categories the language uses for counts 0-1000, plus "other", in CLDR order."""
    script = (
        "const r=new Intl.PluralRules(process.argv[1]);console.log([...new Set(Array.from({length:1001},(_,n)=>r.select(n)))].join(' '))"
    )
    used = set(subprocess.run(["node", "-e", script, code], capture_output=True, text=True, check=True).stdout.split()) | {"other"}
    return ", ".join(form for form in ("zero", "one", "two", "few", "many", "other") if form in used)


def system_prompt(code: str, glossary: dict, keep: list[str], plurals: str) -> str:
    terms = "\n".join(f"- {en} -> {tr.split('|')[0]}" for en, tr in glossary.get("terms", {}).items())
    return f"""You are a native {glossary["name"]} ({code}) software UI writer translating the web interface of SQMeter,
a sky-quality and observatory-safety monitor used by amateur astronomers. Write as a fluent native UI writer
would, not word for word.

Register: {glossary["register"]}
Notes: {glossary.get("notes", "")}

Glossary (use these terms consistently):
{terms}

Keep these exactly as written: {", ".join(keep)}.

Rules:
- Keep every {{placeholder}} exactly; never translate or drop one.
- Keep leading and trailing spaces exactly as in the English: those strings are joined to other text.
- A plural entry is a JSON object; return the forms {plurals} (an explicit "zero" form is optional), using {{count}}.
- Respect a "max N chars" limit in the context note.
- "Unsafe" means conditions are not safe for observing; translate it as "not safe", never as "dangerous".
Reply with one JSON object mapping each key to its translation, and nothing else."""


def translate_request(entries: dict) -> str:
    return "Translate these UI strings. Each has the English and a context note.\n\n" + json.dumps(entries, ensure_ascii=False, indent=1)


def review_request(entries: dict, draft: dict) -> str:
    safety = [key for key in SAFETY_KEYS if key in draft]
    return (
        "Review this draft as a native UI editor. Fix anything that is not fluent, inconsistent with the glossary, "
        "too long for its context, or that changes a placeholder or the meaning. Reply with one JSON object: "
        '{"messages": {<every key>: <final translation>}, "notes": [<short notes on what you changed and why>], '
        '"backTranslations": {<key>: <English back-translation>}} with back-translations for these safety keys: '
        f"{', '.join(safety) or 'none in this batch'}.\n\nSource:\n{json.dumps(entries, ensure_ascii=False, indent=1)}"
        f"\n\nDraft:\n{json.dumps(draft, ensure_ascii=False, indent=1)}"
    )


def call_api(system: str, user: str, model: str = MODEL) -> str:
    key = os.environ.get("ANTHROPIC_API_KEY")
    if not key:
        raise SystemExit("translate.py: set ANTHROPIC_API_KEY")
    body = json.dumps({"model": model, "max_tokens": 16000, "system": system, "messages": [{"role": "user", "content": user}]}).encode()
    request = urllib.request.Request(
        API_URL, data=body, headers={"x-api-key": key, "anthropic-version": "2023-06-01", "content-type": "application/json"}
    )
    with urllib.request.urlopen(request, timeout=600) as response:
        reply = json.loads(response.read())
    return "".join(part.get("text", "") for part in reply.get("content", []))


def parse_json_reply(text: str) -> dict:
    """The JSON object in a reply, with or without a ``` fence."""
    start, end = text.find("{"), text.rfind("}")
    if start < 0 or end < start:
        raise ValueError("the reply has no JSON object")
    return json.loads(text[start : end + 1])


def check_candidate(code: str, messages: dict) -> list[str]:
    """Problems tools/i18n/check.mjs finds in a candidate file."""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as handle:
        json.dump(messages, handle, ensure_ascii=False)
    try:
        out = subprocess.run(
            ["node", str(CHECK), "--lang", code, "--file", handle.name, "--json"], capture_output=True, text=True, check=True
        )
    finally:
        os.unlink(handle.name)
    return [problem["message"] for problem in json.loads(out.stdout or "[]")]


def translate_batch(code: str, system: str, entries: dict, api) -> tuple[dict, dict]:
    """Draft, then review; returns (messages, review)."""
    draft = parse_json_reply(api(system, translate_request(entries)))
    review = parse_json_reply(api(system, review_request(entries, draft)))
    final = review.get("messages") or draft
    missing = [key for key in entries if key not in final]
    if missing:
        raise ValueError(f"{code}: the reply left out {', '.join(missing[:5])}")
    return {key: final[key] for key in entries}, review


def append_review(code: str, keys: list[str], reviews: list[dict]) -> None:
    path = TOOLS / "review" / f"{code}.md"
    lines = ["", f"## Regenerated {datetime.date.today().isoformat()} ({len(keys)} keys)", ""]
    for review in reviews:
        lines += [f"- {note}" for note in review.get("notes", [])]
        lines += [f"- Back-translation `{key}`: {text}" for key, text in review.get("backTranslations", {}).items()]
    with path.open("a", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def translate_language(code: str, api=call_api, dry_run: bool = False, batch: int = BATCH) -> list[str]:
    """Translates one language's changed keys; returns the keys it translated."""
    en, context = read_json(I18N / "en.json"), read_json(I18N / "en.context.json", {})
    path = I18N / "locales" / f"{code}.json"
    messages = read_json(path, {})
    keys = changed_keys(en, messages, translated_from(read_json(RECORD, {}), code))
    if dry_run or not keys:
        return keys
    glossary, keep = read_json(TOOLS / "glossary" / f"{code}.json"), read_json(TOOLS / "glossary" / "_keep.json")["keep"]
    system = system_prompt(code, glossary, keep, plural_forms(code))
    reviews = []
    for start in range(0, len(keys), batch):
        entries = {key: {"english": en[key], "context": context.get(key, "")} for key in keys[start : start + batch]}
        translated, review = translate_batch(code, system, entries, api)
        messages.update(translated)
        reviews.append(review)
    merged = {key: messages[key] for key in en if key in messages}
    problems = check_candidate(code, merged)
    if problems:
        raise SystemExit(f"translate.py: {code} not written, check.mjs found:\n  " + "\n  ".join(problems[:20]))
    write_json(path, merged)
    stamp(code, en, keys)
    append_review(code, keys, reviews)
    return keys


# record.json: {"english": {key: hash}, "languages": {code: {key: hash}}}. "english" is
# the English every language was last brought up to; a language lists only the keys
# it was translated from different English ("-" = never), so the file stays small.
def translated_from(record: dict, code: str) -> dict:
    """The English fingerprint each of a language's keys was translated from."""
    if code not in record.get("languages", {}):
        return {}
    base, own = record.get("english", {}), record["languages"][code]
    return {key: own.get(key, base.get(key)) for key in set(base) | set(own)}


def stamp(code: str, en: dict, keys: list[str]) -> None:
    """Records that `code`'s `keys` are now translated from the current English."""
    record = read_json(RECORD, {})
    effective = {other: translated_from(record, other) for other in record.get("languages", {})}
    effective.setdefault(code, {}).update({key: fingerprint(en[key]) for key in keys})
    base = {key: fingerprint(english) for key, english in en.items()}
    languages_ = {
        other: dict(sorted((key, value or "-") for key, value in ((k, mine.get(k)) for k in en) if (value or "-") != base[key]))
        for other, mine in sorted(effective.items())
    }
    write_json(RECORD, {"english": base, "languages": languages_})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--lang", action="append", help="language code; repeat for several")
    group.add_argument("--all", action="store_true")
    parser.add_argument("--dry-run", action="store_true", help="list the keys that would be translated")
    parser.add_argument("--record", action="store_true", help="mark the current files as translated from the current English")
    args = parser.parse_args()
    en = read_json(I18N / "en.json")
    for code in languages() if args.all else args.lang:
        if args.record:
            stamp(code, en, [key for key in read_json(I18N / "locales" / f"{code}.json", {}) if key in en])
            print(f"{code}: recorded")
            continue
        keys = translate_language(code, dry_run=args.dry_run)
        print(f"{code}: {len(keys)} key(s) {'to translate' if args.dry_run else 'translated'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
