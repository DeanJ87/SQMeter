"""The coding standard's automatic checks (spec 017), one function per tool.

Each returns Findings tagged with the rule ID from
docs/development/coding-standards.md. check.py runs them and compares the
result with the baseline.
"""

from __future__ import annotations

import ast
import json
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "web"

CPP_PATTERNS = ["src/*.cpp", "src/*.h", "include/*.h", "lib/*.cpp", "lib/*.h", "test/*.cpp", "test/*.h", "tools/*.cpp", "tools/*.h"]
PY_PATTERNS = ["tools/*.py"]
PRETTIER_GLOBS = ["src/**/*.{ts,tsx,css}", "tests/**/*.ts", "*.ts", "*.json", ".prettierrc.json"]

# Limits (LIMIT-01..05). TypeScript limits live in web/eslint.config.js.
CPP_FUNCTION_LINES = 60
PY_FUNCTION_LINES = 60
MAX_COMPLEXITY = 15
MAX_PARAMS = 5
MAX_NESTING = 4
CPP_FILE_LINES = 600
PY_FILE_LINES = 400

# STRUCT-01: lib/ is portable - no hardware, RTOS or network headers, no clock reads.
HARDWARE_INCLUDE = re.compile(
    r'^\s*#\s*include\s*[<"](Arduino\.h|esp_[^>"]*|freertos/[^>"]*|driver/[^>"]*|WiFi[^>"]*|Wire\.h|SPI\.h|'
    r"HardwareSerial\.h|Preferences\.h|LittleFS\.h|SPIFFS\.h|FS\.h|AsyncTCP\.h|AsyncUDP\.h|ESPAsyncWebServer\.h|"
    r'PubSubClient\.h|NimBLE[^>"]*|Adafruit[^>"]*|TinyGPS[^>"]*|DNSServer\.h|HTTPClient\.h|Update\.h)[>"]'
)
CLOCK_CALL = re.compile(r"\b(millis|micros)\s*\(\s*\)|\btime\s*\(\s*(nullptr|NULL|0|&)")

# EXC-01: a suppression names the rule and gives a reason.
NOLINT = re.compile(r"NOLINT(NEXTLINE|BEGIN|END)?\b(?P<rest>.*)")
NOLINT_OK = re.compile(r"^\((?P<rules>[^)]+)\)\s*[:\-–]+\s*\S")
NOQA = re.compile(r"#\s*noqa\b(?P<rest>.*)", re.IGNORECASE)
NOQA_OK = re.compile(r"^:\s*[A-Z]+\d+(\s*,\s*[A-Z]+\d+)*\s+[-–]+\s*\S")

HINTS = {
    "FMT-01": "run `python3 tools/quality/check.py --fix` (or `npm run format` in web/)",
    "NAME-01": "C++ types and namespaces are PascalCase",
    "NAME-02": "C++ functions, variables, parameters and members are camelCase",
    "NAME-03": "C++ constexpr/static const constants are UPPER_SNAKE_CASE, enum values PascalCase",
    "NAME-05": "TS: components/types PascalCase, functions/variables camelCase, module constants UPPER_SNAKE_CASE",
    "NAME-09": "Python: snake_case functions/variables, PascalCase classes, UPPER_SNAKE_CASE constants",
    "STRUCT-01": "lib/ must stay portable: move the hardware call to src/ and pass the value in",
    "STRUCT-04": "components get data from the data layer (web/src/hooks, web/src/lib), not fetch/WebSocket",
    "STRUCT-05": "only main.tsx and tests import mocks/ or demo/",
    "LIMIT-01": "function too long: extract named steps",
    "LIMIT-02": "too complex: split the decision into smaller functions or a table",
    "LIMIT-03": "too many parameters: pass a struct/object",
    "LIMIT-04": "file too long: split it by responsibility",
    "LIMIT-05": "nested too deep: return early or extract",
    "SMELL-11": "remove the unused code",
    "SMELL-18": "replace `any` with a real type or `unknown` plus narrowing",
    "ERR-01": "don't swallow the error: handle it, return it or log what failed",
    "EXC-01": "a suppression names the rule and says why on the same line, e.g. `// NOLINT(rule): reason`",
    "LINT-01": "fix the linter finding, or suppress it with a reason (EXC-01)",
    "DASH-02": "map the field or dependency in web/src/dashboard/inventory.json (shown with a test, or notShown with a reason): "
    "python3 tools/dashboard/check.py",
    "DS-COPY": "rewrite the English per DS-20..DS-25 (python3 tools/ui/copy_check.py --list shows each string's type), "
    "or record a reason in tools/ui/copy-exceptions.json",
    "DS-LABEL": "use the one name in tools/i18n/glossary/en.json (DS-27): python3 tools/ui/label_check.py",
    "DS-PATH": "name the tab, card and control as the UI does (web/src/i18n/en.json): python3 tools/docs/ui_paths.py",
    "I18N-05": "format with web/src/i18n/format.ts and read typed numbers with web/src/i18n/parse.ts "
    "(SVG geometry: web/src/lib/svg.ts; option values: Number(value))",
    "I18N-01": "move the text to web/src/i18n/en.json and use t() (or `// i18n-ignore: <reason>`)",
    "I18N-02": "every language file needs exactly the English keys, placeholders and plural forms: "
    "run `node tools/i18n/check.mjs`, then `python3 tools/i18n/translate.py`",
}


@dataclass(frozen=True)
class Finding:
    rule: str
    path: str
    line: int
    message: str


# --- helpers -----------------------------------------------------------------


def tool(name: str) -> str:
    """A tool from this Python's environment first (the pinned venv), then PATH."""
    local = Path(sys.executable).parent / name
    if local.exists():
        return str(local)
    found = shutil.which(name)
    if not found:
        sys.exit(f"check.py: `{name}` not found - pip install -r tools/quality/requirements.txt")
    return found


def tracked(patterns: list[str]) -> list[str]:
    out = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", *patterns],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return [line for line in out.stdout.splitlines() if line and (ROOT / line).exists()]


def run(cmd: list[str], cwd: Path = ROOT) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def rel(path: str, base: Path = ROOT) -> str:
    return os.path.relpath((base / path).resolve(), ROOT)


def is_test(path: str) -> bool:
    return path.startswith("test/") or "/__tests__/" in path or Path(path).name.startswith("test_")


# --- formatters (FMT-01) -----------------------------------------------------


def check_clang_format(files: list[str], fix: bool) -> list[Finding]:
    exe = tool("clang-format")
    if fix:
        run([exe, "-i", *files])
        return []
    out = run([exe, "--dry-run", "--Werror", *files])
    bad = {m.group(1) for m in re.finditer(r"^(.+?):\d+:\d+: (?:error|warning):", out.stderr, re.MULTILINE)}
    return [Finding("FMT-01", rel(p), 1, "not formatted by clang-format") for p in sorted(bad)]


def check_prettier(fix: bool) -> list[Finding]:
    exe = str(WEB / "node_modules" / ".bin" / "prettier")
    if fix:
        run([exe, "--write", "--log-level", "warn", *PRETTIER_GLOBS], cwd=WEB)
        return []
    out = run([exe, "--list-different", *PRETTIER_GLOBS], cwd=WEB)
    return [Finding("FMT-01", rel(p, WEB), 1, "not formatted by Prettier") for p in out.stdout.split()]


def check_ruff_format(fix: bool) -> list[Finding]:
    exe = tool("ruff")
    if fix:
        run([exe, "format", "--quiet", "tools"])
        return []
    out = run([exe, "format", "--check", "tools"])
    return [
        Finding("FMT-01", m.group(1).strip(), 1, "not formatted by ruff format")
        for m in re.finditer(r"^Would reformat: (.+)$", out.stdout, re.MULTILINE)
    ]


# --- linters -----------------------------------------------------------------

ESLINT_RULES = {
    "max-lines": "LIMIT-04",
    "max-lines-per-function": "LIMIT-01",
    "complexity": "LIMIT-02",
    "max-params": "LIMIT-03",
    "max-depth": "LIMIT-05",
    "@typescript-eslint/naming-convention": "NAME-05",
    "no-restricted-globals": "STRUCT-04",
    "no-restricted-imports": "STRUCT-05",
    "@eslint-community/eslint-comments/require-description": "EXC-01",
    "@eslint-community/eslint-comments/no-unlimited-disable": "EXC-01",
    "@typescript-eslint/ban-ts-comment": "EXC-01",
    "@typescript-eslint/no-explicit-any": "SMELL-18",
    "@typescript-eslint/no-unused-vars": "SMELL-11",
    "no-unused-vars": "SMELL-11",
    "no-empty": "ERR-01",
    "no-restricted-syntax": "I18N-05",
}


def check_eslint(fix: bool, targets: tuple[str, ...] = (".",)) -> list[Finding]:
    exe = str(WEB / "node_modules" / ".bin" / "eslint")
    cmd = [exe, "--format", "json", *targets] + (["--fix"] if fix else [])
    out = run(cmd, cwd=WEB)
    try:
        results = json.loads(out.stdout)
    except json.JSONDecodeError:
        sys.exit(f"check.py: ESLint failed:\n{out.stderr or out.stdout}")
    findings = []
    for result in results:
        path = rel(result["filePath"], WEB)
        for msg in result["messages"]:
            rule = ESLINT_RULES.get(msg.get("ruleId") or "", "LINT-01")
            findings.append(Finding(rule, path, msg.get("line", 1), f"{msg['message']} [{msg.get('ruleId')}]"))
    return findings


def json_tool(rule: str, script: str) -> list[Finding]:
    """A checker that prints [{file, line, message}] with --json."""
    out = run([sys.executable, script, "--json"])
    try:
        return [Finding(rule, item["file"], item["line"], item["message"]) for item in json.loads(out.stdout or "[]")]
    except json.JSONDecodeError:
        sys.exit(f"check.py: {script} failed:\n{out.stderr or out.stdout}")


def check_dashboard() -> list[Finding]:
    """DASH-02 dashboard inventory (spec 025); DS-COPY English copy, DS-LABEL one name per thing and
    DS-PATH docs paths that name the UI as it is (spec 026)."""
    return (
        json_tool("DASH-02", "tools/dashboard/check.py")
        + json_tool("DS-COPY", "tools/ui/copy_check.py")
        + json_tool("DS-LABEL", "tools/ui/label_check.py")
        + json_tool("DS-PATH", "tools/docs/ui_paths.py")
    )


def check_i18n() -> list[Finding]:
    """I18N-01 hard-coded UI text and I18N-02 incomplete translations (spec 023)."""
    findings = []
    out = run(["node", "tools/i18n/literals.mjs", "--json"])
    try:
        for item in json.loads(out.stdout or "[]"):
            findings.append(Finding("I18N-01", item["file"], item["line"], f'hard-coded UI text "{item["text"]}"'))
    except json.JSONDecodeError:
        sys.exit(f"check.py: tools/i18n/literals.mjs failed:\n{out.stderr or out.stdout}")
    out = run(["node", "tools/i18n/check.mjs", "--json"])
    try:
        for item in json.loads(out.stdout or "[]"):
            findings.append(Finding("I18N-02", item["file"], 1, item["message"]))
    except json.JSONDecodeError:
        sys.exit(f"check.py: tools/i18n/check.mjs failed:\n{out.stderr or out.stdout}")
    # Generated files: the device message templates and the context notes.
    for script, target in (("gen_device_catalog.py", "web/src/i18n/en.json"), ("context.py", "web/src/i18n/en.context.json")):
        out = run([sys.executable, f"tools/i18n/{script}", "--check"])
        if out.returncode:
            findings.append(
                Finding("I18N-02", target, 1, f"out of date: run python3 tools/i18n/{script} ({(out.stdout or out.stderr).strip()[:200]})")
            )
    return findings


def ruff_rule(code: str) -> str:
    if code == "C901":
        return "LIMIT-02"
    if code == "PLR0913":
        return "LIMIT-03"
    if code.startswith("N"):
        return "NAME-09"
    if code in ("F401", "F841"):
        return "SMELL-11"
    if code in ("S110", "S112", "E722"):
        return "ERR-01"
    return "LINT-01"


def check_ruff(fix: bool, target: str = "tools") -> list[Finding]:
    exe = tool("ruff")
    out = run([exe, "check", "--output-format", "json", target] + (["--fix"] if fix else []))
    try:
        results = json.loads(out.stdout or "[]")
    except json.JSONDecodeError:
        sys.exit(f"check.py: ruff failed:\n{out.stderr}")
    return [Finding(ruff_rule(r["code"]), rel(r["filename"]), r["location"]["row"], f"{r['message']} [{r['code']}]") for r in results]


def tidy_rule(check: str, message: str) -> str:
    if check != "readability-identifier-naming":
        return "LINT-01"
    if re.search(r"invalid case style for (constexpr|global constant|static constant|constant|enum constant)", message):
        return "NAME-03"
    if re.search(r"invalid case style for (class|struct|enum|namespace|type alias|union)\b", message):
        return "NAME-01"
    return "NAME-02"


def tidy_args() -> list[str]:
    includes = sorted(str(p) for p in (ROOT / "lib").glob("*/include"))
    args = ["--quiet", "--extra-arg=-std=c++17"] + [f"--extra-arg=-I{p}" for p in includes]
    json_lib = next((ROOT / ".pio" / "libdeps" / "native").glob("ArduinoJson*/src"), None)
    if json_lib is None:
        sys.exit("check.py: ArduinoJson not found - run `pio pkg install -e native` first (or use --fast)")
    args.append(f"--extra-arg=-I{json_lib}")
    if sys.platform == "darwin":
        sdk = run(["xcrun", "--show-sdk-path"]).stdout.strip()
        args += [f"--extra-arg=-isysroot{sdk}", f"--extra-arg=-isystem{sdk}/usr/include/c++/v1"]
    return args


def check_clang_tidy(files: list[str], scope: str = "lib/") -> list[Finding]:
    exe = tool("clang-tidy")
    args = tidy_args()
    sources = [f for f in files if f.startswith(scope) and f.endswith(".cpp")]

    def one(path: str) -> str:
        return run([exe, *args, path, "--"]).stdout

    findings = set()
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        for output in pool.map(one, sources):
            for m in re.finditer(r"^(.+?):(\d+):\d+: (?:warning|error): (.+?) \[([\w.,-]+)\]$", output, re.MULTILINE):
                path = rel(m.group(1))
                if path.startswith(scope):
                    findings.add(Finding(tidy_rule(m.group(4), m.group(3)), path, int(m.group(2)), f"{m.group(3)} [{m.group(4)}]"))
    return sorted(findings, key=lambda f: (f.path, f.line))


# --- limits (LIMIT-01..05) -----------------------------------------------------


def check_lizard(cpp: list[str], py: list[str]) -> list[Finding]:
    import lizard  # noqa: PLC0415 - only needed here, and the pinned venv provides it

    findings = []
    for info in lizard.analyze_files(cpp + py):
        path = rel(info.filename)
        python = path.endswith(".py")
        for fn in info.function_list:
            where = f"{fn.name}()"
            if fn.nloc > (PY_FUNCTION_LINES if python else CPP_FUNCTION_LINES) and not is_test(path):
                findings.append(Finding("LIMIT-01", path, fn.start_line, f"{where} has {fn.nloc} lines"))
            if python:
                continue  # complexity and parameters come from ruff (C901, PLR0913)
            if fn.cyclomatic_complexity > MAX_COMPLEXITY:
                findings.append(Finding("LIMIT-02", path, fn.start_line, f"{where} has complexity {fn.cyclomatic_complexity}"))
            if fn.parameter_count > MAX_PARAMS:
                findings.append(Finding("LIMIT-03", path, fn.start_line, f"{where} has {fn.parameter_count} parameters"))
    return findings


NESTING = (ast.If, ast.For, ast.AsyncFor, ast.While, ast.With, ast.AsyncWith, ast.Try, ast.Match)


def python_nesting(path: str) -> list[Finding]:
    tree = ast.parse((ROOT / path).read_text(encoding="utf-8"))
    findings = []

    def walk(node: ast.AST, depth: int, function: str | None) -> None:
        for child in ast.iter_child_nodes(node):
            if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)):
                walk(child, 0, child.name)
                continue
            deeper = depth + 1 if isinstance(child, NESTING) else depth
            if function and isinstance(child, NESTING) and deeper == MAX_NESTING + 1:
                findings.append(Finding("LIMIT-05", path, child.lineno, f"{function}() nests {deeper} blocks deep"))
            walk(child, deeper, function)

    walk(tree, 0, None)
    return findings


def check_file_lengths(cpp: list[str], py: list[str]) -> list[Finding]:
    findings = []
    for path, limit in [(p, CPP_FILE_LINES) for p in cpp] + [(p, PY_FILE_LINES) for p in py]:
        lines = len((ROOT / path).read_text(encoding="utf-8", errors="replace").splitlines())
        if lines > limit:
            findings.append(Finding("LIMIT-04", path, 1, f"{lines} lines (limit {limit})"))
    return findings


# --- structure and suppressions --------------------------------------------------


def strip_comment(line: str) -> str:
    return line.split("//", 1)[0]


def check_lib_purity(cpp: list[str], scope: str = "lib/") -> list[Finding]:
    findings = []
    for path in (p for p in cpp if p.startswith(scope)):
        for number, line in enumerate((ROOT / path).read_text(encoding="utf-8").splitlines(), 1):
            if HARDWARE_INCLUDE.search(line):
                findings.append(Finding("STRUCT-01", path, number, f"hardware/platform include: {line.strip()}"))
            elif CLOCK_CALL.search(strip_comment(line)):
                findings.append(Finding("STRUCT-01", path, number, "reads the clock - pass the time in"))
    return findings


def check_suppressions(cpp: list[str], py: list[str]) -> list[Finding]:
    findings = []
    for path in cpp:
        for number, line in enumerate((ROOT / path).read_text(encoding="utf-8").splitlines(), 1):
            m = NOLINT.search(line)
            if m and m.group(1) != "END" and not NOLINT_OK.match(m.group("rest").strip()):
                findings.append(Finding("EXC-01", path, number, "NOLINT without a rule and a reason"))
    for path in py:
        for number, line in enumerate((ROOT / path).read_text(encoding="utf-8").splitlines(), 1):
            m = NOQA.search(line)
            if m and not NOQA_OK.match(m.group("rest").strip()):
                findings.append(Finding("EXC-01", path, number, "noqa without a rule code and a reason"))
    return findings
