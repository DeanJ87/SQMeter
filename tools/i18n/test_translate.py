"""Tests for tools/i18n/translate.py and build_packs.py (specs/023-i18n).

python3 -m unittest discover -s tools/i18n -p 'test_*.py'
"""

import gzip
import hashlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_packs  # noqa: E402 - imported after the sys.path insert above
import translate  # noqa: E402 - imported after the sys.path insert above

REAL_I18N = translate.I18N
REAL_TOOLS = translate.TOOLS


def echo_api(calls: list):
    """A fake API: drafts are the English itself (valid placeholders), the review keeps the draft."""

    def api(system: str, user: str) -> str:
        calls.append((system, user))
        if user.startswith("Review"):
            draft = json.loads(user[user.index("Draft:\n") + len("Draft:\n") :])
            return json.dumps({"messages": draft, "notes": ["looks fine"], "backTranslations": {"safetyCard.safe": "Safe"}})
        entries = json.loads(user[user.index("{") :])
        return "```json\n" + json.dumps({key: entry["english"] for key, entry in entries.items()}) + "\n```"

    return api


class Workspace(unittest.TestCase):
    """A copy of web/src/i18n and tools/i18n so tests never touch the real files."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        shutil.copytree(REAL_I18N, self.tmp / "i18n")
        shutil.copytree(REAL_TOOLS / "glossary", self.tmp / "tools/glossary")
        (self.tmp / "tools/review").mkdir(parents=True)
        shutil.copy(REAL_TOOLS / "record.json", self.tmp / "tools/record.json")
        translate.I18N, translate.TOOLS, translate.RECORD = self.tmp / "i18n", self.tmp / "tools", self.tmp / "tools/record.json"
        self.en = json.loads((translate.I18N / "en.json").read_text())

    def tearDown(self):
        translate.I18N, translate.TOOLS, translate.RECORD = REAL_I18N, REAL_TOOLS, REAL_TOOLS / "record.json"
        shutil.rmtree(self.tmp)

    def locale(self, code: str) -> dict:
        return json.loads((translate.I18N / "locales" / f"{code}.json").read_text())

    def drop_keys(self, code: str, keys: list[str]) -> None:
        messages = {key: value for key, value in self.locale(code).items() if key not in keys}
        translate.write_json(translate.I18N / "locales" / f"{code}.json", messages)


class TranslateTest(Workspace):
    def test_nothing_to_do_when_the_record_is_current(self):
        self.assertEqual(translate.translate_language("es", api=echo_api([])), [])

    def test_only_missing_and_changed_keys_are_sent(self):
        self.drop_keys("es", ["alertsBell.clear"])
        en = dict(self.en, **{"alertsBell.pause": "Pause now"})
        translate.write_json(translate.I18N / "en.json", en)
        calls = []
        keys = translate.translate_language("es", api=echo_api(calls))
        self.assertEqual(sorted(keys), ["alertsBell.clear", "alertsBell.pause"])
        sent = json.loads(calls[0][1][calls[0][1].index("{") :])
        self.assertEqual(sorted(sent), ["alertsBell.clear", "alertsBell.pause"])
        self.assertIn("context", sent["alertsBell.clear"])

    def test_prompt_carries_register_glossary_keep_list_and_plurals(self):
        self.drop_keys("pl", ["alertsBell.clear"])
        calls = []
        translate.translate_language("pl", api=echo_api(calls))
        system = calls[0][0]
        glossary = json.loads((translate.TOOLS / "glossary/pl.json").read_text())
        self.assertIn(glossary["register"], system)
        self.assertIn("punkt rosy", system)
        self.assertIn("N.I.N.A.", system)
        self.assertIn("one, few, many, other", system)
        self.assertTrue(calls[1][1].startswith("Review"), "a second, review call follows the draft")

    def test_written_in_english_key_order_and_recorded(self):
        self.drop_keys("de", ["alertsBell.clear"])
        translate.translate_language("de", api=echo_api([]))
        self.assertEqual(list(self.locale("de")), list(self.en))
        self.assertEqual(translate.translate_language("de", api=echo_api([])), [])

    def test_review_notes_appended(self):
        self.drop_keys("fr", ["safetyCard.safe"])
        translate.translate_language("fr", api=echo_api([]))
        note = (translate.TOOLS / "review/fr.md").read_text()
        self.assertIn("looks fine", note)
        self.assertIn("Back-translation `safetyCard.safe`: Safe", note)

    def test_a_reply_that_breaks_a_placeholder_is_not_written(self):
        self.drop_keys("it", ["alertsBell.newAlertTitle"])
        before = self.locale("it")

        def bad_api(system, user):
            if user.startswith("Review"):
                return json.dumps({"messages": {"alertsBell.newAlertTitle": "Nuovo avviso"}})
            return json.dumps({"alertsBell.newAlertTitle": "Nuovo avviso"})

        with self.assertRaises(SystemExit) as raised:
            translate.translate_language("it", api=bad_api)
        self.assertIn("placeholders", str(raised.exception))
        self.assertEqual(self.locale("it"), before)

    def test_a_reply_missing_keys_is_rejected(self):
        self.drop_keys("nl", ["alertsBell.clear", "alertsBell.pause"])
        with self.assertRaises(ValueError):
            translate.translate_language("nl", api=lambda system, user: json.dumps({"alertsBell.clear": "Wissen"}))

    def test_record_keeps_other_languages_behind_when_english_moves_on(self):
        en = dict(self.en, **{"alertsBell.pause": "Pause now"})
        translate.write_json(translate.I18N / "en.json", en)
        translate.stamp("es", en, ["alertsBell.pause"])
        record = json.loads(translate.RECORD.read_text())
        self.assertEqual(record["english"]["alertsBell.pause"], translate.fingerprint("Pause now"))
        self.assertIn("alertsBell.pause", record["languages"]["fr"], "French was translated from the old English")
        self.assertNotIn("alertsBell.pause", record["languages"]["es"])
        self.assertEqual(translate.translate_language("fr", dry_run=True), ["alertsBell.pause"])

    def test_parse_reply_with_or_without_fence(self):
        self.assertEqual(translate.parse_json_reply('```json\n{"a": 1}\n```'), {"a": 1})
        self.assertEqual(translate.parse_json_reply('{"a": 1}'), {"a": 1})
        with self.assertRaises(ValueError):
            translate.parse_json_reply("no json here")


class BuildPacksTest(unittest.TestCase):
    def setUp(self):
        self.out = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.out)

    def test_files_sidecars_and_manifest(self):
        manifest = build_packs.build("1.2.3", self.out)
        codes = sorted(entry["code"] for entry in manifest["languages"])
        self.assertEqual(codes, sorted(translate.languages()))
        for entry in manifest["languages"]:
            data = (self.out / entry["file"]).read_bytes()
            self.assertEqual(data[:2], b"\x1f\x8b")
            self.assertLessEqual(len(data), build_packs.MAX_FILE_BYTES)
            body = json.loads(gzip.decompress(data))
            self.assertEqual((body["lang"], body["version"]), (entry["code"], "1.2.3"))
            digest, size = (self.out / f'{entry["file"]}.sha256').read_text().split()
            self.assertEqual((digest, int(size)), (hashlib.sha256(data).hexdigest(), len(data)))

    def test_output_is_reproducible(self):
        first = build_packs.pack("es", "1.0.0", {"a": "b"})
        self.assertEqual(first, build_packs.pack("es", "1.0.0", {"a": "b"}))

    def test_too_big_fails(self):
        locales = self.out / "locales"
        locales.mkdir()
        noise = {f"k{i}": hashlib.sha256(str(i).encode()).hexdigest() * 4 for i in range(2000)}
        (locales / "xx.json").write_text(json.dumps(noise))
        with self.assertRaises(SystemExit):
            build_packs.build("1.0.0", self.out / "dist", locales)


if __name__ == "__main__":
    unittest.main()
