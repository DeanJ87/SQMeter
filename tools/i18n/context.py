#!/usr/bin/env python3
"""Context notes for translators (specs/023-i18n FR-020).

Fills web/src/i18n/en.context.json with a note for every key that has none:
where the text appears (page and kind of control, found from how the code uses
the key), what the domain terms in it mean, and a length limit where the space
is tight. Existing notes are kept, so a note improved by hand stays.

  python3 tools/i18n/context.py           add missing notes, drop notes for removed keys
  python3 tools/i18n/context.py --check   fail if any key has no note
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "web/src"
EN = SRC / "i18n/en.json"
CONTEXT = SRC / "i18n/en.context.json"

PLACES = {
    "a11y": "Screen-reader text (not shown)",
    "alertsBell": "Header alerts bell and its flyout",
    "alpaca": "Alpaca page (ASCOM Alpaca devices for imaging apps)",
    "alpacaClients": "Alpaca page and Alerts settings: the imaging app's connection state",
    "astro": "Sun & Moon card: sky phase or moon phase name",
    "common": "Shared, used on several pages",
    "dashboard": "Dashboard (live readings)",
    "device": "Sent by the device and shown in the UI",
    "language": "Settings -> Device -> Language",
    "layout": "Page header and navigation",
    "masonry": "Dashboard card arranging (drag and move buttons)",
    "nightChart": "Sun & Moon card: the night chart",
    "notFound": "Page-not-found screen",
    "safetyCard": "Dashboard: Safety Monitor card (safe/unsafe verdict and its history)",
    "settings": "Settings",
    "settings.alertSchedule": "Settings -> Alerts: when alerts are sent, paused and resumed",
    "settings.alerts": "Settings -> Alerts",
    "settings.depLabels": "Settings: names of settings in the 'Saving makes these inactive' note",
    "settings.device": "Settings -> Device",
    "settings.network": "Settings -> Network (WiFi, MQTT, Home Assistant)",
    "settings.restart": "Settings: list of what needs a restart",
    "settings.safety": "Settings -> Safety (the rules that make the observatory unsafe)",
    "settings.sensors": "Settings -> Sensors",
    "settings.tabs": "Settings tab names",
    "settings.time": "Settings -> Time & Location",
    "settingsDeps": "Settings: why a setting is inactive, and the button that fixes it",
    "sunMoonCard": "Dashboard: Sun & Moon card",
    "system": "System page (device diagnostics)",
    "timezone": "Time zone list",
    "toast": "Pop-up message",
    "ui": "Shared controls",
    "units": "Short unit formats for durations; keep them short",
    "updates": "Updates page (firmware updates)",
    "validation": "Settings: validation error under a field",
    "webSocket": "Pop-up when the live connection drops or returns",
    "wifiScan": "WiFi network scan",
    "wifiSetup": "WiFi setup page (first setup from the device's hotspot)",
}

# Where in the UI a key is used, from the code around t('key').
KINDS = [
    (r"label=\{t\('KEY'", "a field or control label", 1.6),
    (r"hint=\{t\('KEY'", "a help text shown with a '?' tip", None),
    (r"title=\{t\('KEY'", "a card or section title", 1.6),
    (r"busyLabel=\{t\('KEY'", "a button label while it works", 1.6),
    (r"ariaLabel=\{t\('KEY'|aria-label=\{t\('KEY'", "read by screen readers only", None),
    (r"placeholder=\{t\('KEY'", "placeholder text inside an empty field", None),
    (r"message: t\('KEY'", "a pop-up message", None),
    (r"<(Button|ActionButton|button)[^>]*>\s*\{t\('KEY'", "a button", 1.5),
    (r"<Pill[^>]*>\{t\('KEY'|StatusBadge[^>]*label=\{t\('KEY'|tone: '[^']+', label: t\('KEY'", "a small status badge", 1.5),
    (r"<Note[^>]*>\s*\{t\('KEY'|<p[^>]*>\s*\{t\('KEY'", "a sentence in a note", None),
    (r"<option[^>]*>\s*\{t\('KEY'|label: t\('KEY'", "an option in a list or a short label", 1.6),
    (r"<h[1-6][^>]*>\s*\{t\('KEY'", "a heading", 1.6),
    (r"metric-label[^>]*>\{t\('KEY'|MetricTile label=\{t\('KEY'|ReadingRow label=\{t\('KEY'|InfoRow label=\{t\('KEY'", "a short label above or beside a reading", 1.5),
    (r"title: t\('KEY'", "an alert title", None),
    (r"(text|description|hint|note): t\('KEY'", "a sentence shown under or beside a control", None),
    (r"\w+: t\('KEY'", "a label in a list or table", 1.6),
    (r"return t\('KEY'", "text the UI builds for a status or description", None),
    (r"\{t\('KEY'", "text on the page", None),
]

# What the text is, for keys whose code use doesn't say (lists, tables, device text).
AREA_KINDS = {
    "device.settings": "an error the device returns when it refuses a settings change",
    "device.safety": "a reason the observatory is unsafe (dashboard, Alpaca, history)",
    "device.alert": "an alert title or message, shown in the alert history under the bell",
    "device.api": "an error the device returns for an action (test, calibrate, restart...)",
    "device.ota": "an error shown while updating the firmware",
    "device.language": "an error about the language file",
    "safetyCard.reason": "a lowercase reason in the safety history list, joined with commas",
    "safetyCard.boot": "a lowercase reason the device restarted, shown in the history",
    "settings.depLabels": "the name of a setting, in a list",
    "settings.tabs": "a tab name; max 16 chars",
    "timezone": "a time zone name; keep the city names and abbreviations",
    "units": "a compact duration (d = days, h = hours, m = minutes, s = seconds)",
    "astro": "a sky or moon phase name",
}

TERMS = {
    "SQM": "SQM = sky quality in mag/arcsec² - keep 'SQM'",
    "NELM": "NELM = naked-eye limiting magnitude - keep 'NELM'",
    "Bortle": "Bortle = the Bortle dark-sky scale (1-9) - keep 'Bortle'",
    "Alpaca": "ASCOM Alpaca, the protocol imaging apps use - keep 'Alpaca'",
    "N.I.N.A.": "N.I.N.A. is an imaging app - keep the name",
    "imaging app": "imaging app = astrophotography software such as N.I.N.A. that controls the telescope and camera",
    "safety monitor": "safety monitor = the Alpaca device that tells imaging apps whether it is safe to observe",
    "weather device": "weather device = the Alpaca ObservingConditions device (weather readings)",
    "unsafe": "unsafe = conditions not safe for the observatory to be open (roof should close)",
    "dew point": "dew point = temperature at which dew forms on optics",
    "MQTT": "MQTT = home-automation messaging protocol - keep 'MQTT'",
    "safe delay": "safe delay = how long it must stay safe before reporting safe again",
    "rain clear delay": "rain clear delay = how long after the last drop it still counts as raining",
    "arm": "armed/paused refers to whether alerts are being sent",
    "Pushover": "Pushover and ntfy are push-notification services - keep the names",
    "RG-15": "RG-15 is the Hydreon rain sensor - keep the name",
    "TSL2591": "TSL2591 (light), MLX90614 (infrared sky temperature) and BME280 (temperature/humidity/pressure) are sensor chips - keep the names",
    "GPIO": "GPIO = an ESP32 pin number - keep 'GPIO'",
    "{": "Keep every {placeholder} exactly; values are filled in by the UI",
}


def uses(key: str, sources: dict[str, str]) -> list[tuple[str, str]]:
    found = []
    for path, text in sources.items():
        for m in re.finditer(re.escape(f"t('{key}'"), text):
            start = text.rfind("\n", 0, max(0, m.start() - 200))
            found.append((path, text[start : m.end() + 5]))
    return found


def kind_of(key: str, snippets: list[str]) -> tuple[str | None, float | None]:
    for pattern, kind, factor in KINDS:
        rx = re.compile(pattern.replace("KEY", re.escape(key)), re.S)
        if any(rx.search(s) for s in snippets):
            return kind, factor
    return None, None


def place_of(key: str) -> str:
    parts = key.split(".")
    for n in range(len(parts) - 1, 0, -1):
        if ".".join(parts[:n]) in PLACES:
            return PLACES[".".join(parts[:n])]
    return "Web UI"


def note_for(key: str, value, sources: dict[str, str]) -> str:
    text = value if isinstance(value, str) else " ".join(value.values())
    snippets = [s for _, s in uses(key, sources)]
    kind, factor = kind_of(key, snippets)
    area = next((a for a in sorted(AREA_KINDS, key=len, reverse=True) if key.startswith(a + ".")), None)
    if area:
        kind, factor = AREA_KINDS[area], None
    note = place_of(key)
    if kind:
        note += f": {kind}"
    if isinstance(value, dict):
        note += ". Plural forms: {count} is the number"
    terms = [hint for term, hint in TERMS.items() if term in text]
    if terms:
        note += ". " + "; ".join(terms)
    if factor and isinstance(value, str) and len(value) <= 40:
        note += f"; max {max(12, round(len(value) * factor))} chars"
    return note + "."


def build() -> tuple[dict, dict]:
    en = json.loads(EN.read_text(encoding="utf-8"))
    context = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.exists() else {}
    sources = {
        str(p.relative_to(SRC)): p.read_text(encoding="utf-8")
        for p in SRC.rglob("*.ts*")
        if "__tests__" not in p.parts and "i18n" not in p.parts
    }
    out = {k: context[k] for k in en if context.get(k, "").strip()}
    for key, value in en.items():
        if key not in out:
            out[key] = note_for(key, value, sources)
    return en, dict(sorted(out.items()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail if a key has no note")
    args = parser.parse_args()
    en, context = build()
    current = json.loads(CONTEXT.read_text(encoding="utf-8")) if CONTEXT.exists() else {}
    missing = [k for k in en if not current.get(k, "").strip()]
    if args.check:
        for k in missing:
            print(f"no context note for {k}: run python3 tools/i18n/context.py")
        return 1 if missing else 0
    CONTEXT.write_text(json.dumps(context, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(missing)} note(s) added, {len(context)} keys")
    return 0


if __name__ == "__main__":
    sys.exit(main())
