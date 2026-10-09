#!/usr/bin/env node
// I18N-01: no hard-coded user-facing text in the web UI (specs/023-i18n FR-001).
// Flags JSX text, user-facing JSX attributes (label, title, hint, ...) and
// prose-like string literals outside t(). Suppress a line with
//   // i18n-ignore: <reason>
// on the same or the previous line (EXC-01: the reason is required).
//
//   node tools/i18n/literals.mjs [--json] [files...]
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const WEB = path.join(ROOT, 'web');
const ts = createRequire(path.join(WEB, 'package.json'))('typescript');

// Not part of the translated device UI (research.md D3).
const SKIP_DIRS = new Set(['__tests__', 'test', 'mocks', 'demo', 'types', 'i18n']);
const USER_ATTR =
  /^(label|title|hint|placeholder|alt|aria-label|ariaLabel|aria-description|aria-valuetext|busyLabel|message|description|heading|caption|summary|emptyText|fixLabel|confirmLabel|helpText|subtitle|actionLabel|tooltip|legend)$/;
const SKIP_PROP =
  /^(class|className|id|key|type|href|src|role|variant|tone|icon|name|for|htmlFor|rel|target|method|autocomplete|autoComplete|inputMode|pattern|dataField|size|align|value|step|min|max|d|viewBox|fill|stroke|xmlns|path|route|channel|event|kind|unit|field|setting|code|tab|anchor|mode|level|state|status|testId|style|color|font|format|accept|lang|dir)$/;
// Names and units that are the same in every language.
const KEEP =
  /^(SQMeter|SQMeter Demo|N\.I\.N\.A\.|ASCOM Alpaca|Alpaca|MQTT|ntfy|Pushover|Home Assistant|GitHub|NTP|GPS|BME280|TSL2591|MLX90614|RG-15|ESP32|UTC|SSID|IP|OK)$/;
// Units are the same in every language (units are their own setting).
const UNITS = /^(mag\s*\/\s*arcsec²|hPa|m\/s|km\/h|mm\/h|mm|°C|°|%|lux|dBm|ms|s|min|h|KB|MB|Hz|kHz|V|SQM|NELM|HDOP|RSSI)$/;
const SAFE_CALLS = new Set(['t', 'tMaybe', 'require', 'fetch', 'route', 'querySelector', 'getElementById', 'matchMedia']);
const SAFE_METHODS =
  /^(log|warn|error|debug|info|getItem|setItem|removeItem|get|has|set|delete|addEventListener|removeEventListener|startsWith|endsWith|includes|split|replace|replaceAll|querySelector|querySelectorAll|closest|matchMedia|setAttribute|getAttribute|removeAttribute|append|toLocaleTimeString|toLocaleDateString|toLocaleString|test|match|send|postMessage|getEntriesByName)$/;

const STOPWORD = /\b(the|is|are|a|an|to|of|or|and|in|on|for|with|no|not|use|be|it|this|that|your|you)\b/i;

export function isProse(text) {
  const v = text.trim().replace(/\$?\{\w*\}/g, 'x');
  if (!/[A-Za-z]{2,}/.test(v) || KEEP.test(v)) return false;
  if (/^[A-Z][a-z]+(\.\.\.|…)$/.test(v)) return true; // "Saving..."
  const words = v.split(/\s+/).filter((w) => /[A-Za-z]{2,}/.test(w));
  if (words.length >= 3 && STOPWORD.test(v) && !/[{};=<>]/.test(v)) return true; // sentences, even lowercase or with dots
  if (words.length >= 2 && /[a-z][.!?]$/.test(v)) return true;
  if (/^[a-z]{2,}( [a-z']{2,})+[.!?]?$/.test(v)) return true; // lowercase prose (no hyphens, unlike class lists)
  if (/^[a-z0-9_\-./:#?=&%${}@ ]+$/.test(v)) return false; // class lists, ids, paths, enum values
  if (/^[A-Z0-9_]+$/.test(v)) return false; // constants, acronyms
  if (!/\s/.test(v) && /[-/._:@[\]()]/.test(v)) return false; // Content-Type, en-GB, selectors
  if (/^(https?:|mailto:|\/|#|\.|\(|\[)/.test(v)) return false;
  if (/^[A-Z][a-z]+[A-Z]/.test(v) && !/\s/.test(v)) return false; // CamelCase identifiers
  return /^[A-Z]/.test(v) || /\s/.test(v) || /[.!?…]$/.test(v);
}

function listFiles(dir, out = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (!SKIP_DIRS.has(entry.name)) listFiles(p, out);
    } else if (/\.(ts|tsx)$/.test(entry.name) && !/\.d\.ts$|\.test\.|\.spec\./.test(entry.name)) out.push(p);
  }
  return out;
}

function skipContext(node, sf) {
  const p = node.parent;
  if (!p) return true;
  if (ts.isImportDeclaration(p) || ts.isExportDeclaration(p) || ts.isLiteralTypeNode(p) || ts.isExternalModuleReference(p)) return true;
  if (ts.isCallExpression(p) && ts.isIdentifier(p.expression) && SAFE_CALLS.has(p.expression.text)) return true;
  if (ts.isCallExpression(p) && ts.isPropertyAccessExpression(p.expression) && SAFE_METHODS.test(p.expression.name.text)) return true;
  if (ts.isBinaryExpression(p) && ['===', '!==', '==', '!=', 'in'].includes(ts.tokenToString(p.operatorToken.kind))) return true;
  if (ts.isCaseClause(p) || ts.isElementAccessExpression(p) || ts.isEnumMember(p)) return true;
  if (ts.isPropertyAssignment(p) && p.name === node) return true;
  if (ts.isPropertyAssignment(p) && SKIP_PROP.test(p.name.getText(sf).replace(/['"]/g, ''))) return true;
  if (ts.isNewExpression(p) && ts.isIdentifier(p.expression) && /RegExp|URL|Intl|Date|WebSocket|URLSearchParams|Error/.test(p.expression.text)) return true;
  if (ts.isJsxAttribute(p) && !USER_ATTR.test(p.name.getText(sf))) return true;
  return false;
}

export function scan(file) {
  const src = fs.readFileSync(file, 'utf8');
  const kind = file.endsWith('.tsx') ? ts.ScriptKind.TSX : ts.ScriptKind.TS;
  const sf = ts.createSourceFile(file, src, ts.ScriptTarget.Latest, true, kind);
  const lines = src.split('\n');
  const ignored = (line) => /i18n-ignore:\s*\S/.test(lines[line] ?? '') || /i18n-ignore:\s*\S/.test(lines[line - 1] ?? '');
  const findings = [];
  const add = (node, text) => {
    const { line } = sf.getLineAndCharacterOfPosition(node.getStart(sf));
    if (!ignored(line)) findings.push({ file: path.relative(ROOT, file), line: line + 1, text: text.trim().slice(0, 80) });
  };
  const visit = (node) => {
    if (ts.isJsxText(node)) {
      // Any word in JSX text is UI text, even a fragment between expressions
      // ("{a} for {b}"); units and names are the exceptions.
      const text = node.text.replace(/\s+/g, ' ').trim();
      if (/[A-Za-z]{2,}/.test(text) && !KEEP.test(text) && !UNITS.test(text)) add(node, node.text);
      return;
    }
    if (ts.isJsxAttribute(node) && node.initializer && ts.isStringLiteral(node.initializer)) {
      if (USER_ATTR.test(node.name.getText(sf)) && isProse(node.initializer.text)) add(node, node.initializer.text);
      return;
    }
    if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) {
      if (isProse(node.text) && !skipContext(node, sf)) add(node, node.text);
      return;
    }
    if (ts.isTemplateExpression(node)) {
      const text = node.head.text + node.templateSpans.map((s) => ' ' + s.literal.text).join('');
      if (isProse(text) && /[A-Za-z]{2,}\s+[A-Za-z]{2,}/.test(text) && !skipContext(node, sf)) add(node, text);
    }
    ts.forEachChild(node, visit);
  };
  visit(sf);
  return findings;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const json = args.includes('--json');
  const files = args.filter((a) => !a.startsWith('--')).map((f) => path.resolve(f));
  const findings = (files.length ? files : listFiles(path.join(WEB, 'src'))).flatMap(scan);
  if (json) process.stdout.write(JSON.stringify(findings) + '\n');
  else {
    for (const f of findings) console.log(`${f.file}:${f.line}: I18N-01 hard-coded UI text "${f.text}" - move it to web/src/i18n/en.json and use t()`);
    console.log(findings.length ? `${findings.length} hard-coded string(s)` : 'OK: no hard-coded UI text');
  }
  process.exit(findings.length && !json ? 1 : 0);
}
