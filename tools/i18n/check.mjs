#!/usr/bin/env node
// Translation checks (specs/023-i18n FR-002..FR-004, FR-020..FR-022).
//
//   node tools/i18n/check.mjs            completeness, placeholders, plurals, context, glossaries
//   node tools/i18n/check.mjs --json     the same as JSON [{file, message}] (tools/quality I18N-02)
//   node tools/i18n/check.mjs --review   also print review flags: glossary misses, possible
//                                        untranslated English, length over the context limit
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const I18N = path.join(ROOT, 'web/src/i18n');
const LOCALES = path.join(I18N, 'locales');
const GLOSSARY = path.join(ROOT, 'tools/i18n/glossary');
const rel = (p) => path.relative(ROOT, p);
// Names, protocols and units that stay as they are in every language.
const SHARED_KEEP = (() => {
  try {
    return JSON.parse(fs.readFileSync(path.join(GLOSSARY, '_keep.json'), 'utf8')).keep;
  } catch {
    return [];
  }
})();

// The supported languages: web/src/i18n/languages.ts is the source.
export const languageCodes = () =>
  [...fs.readFileSync(path.join(I18N, 'languages.ts'), 'utf8').matchAll(/code: '([^']+)'/g)].map((m) => m[1]).filter((c) => c !== 'en');

const readJson = (file, problems) => {
  try {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch (error) {
    problems.push({ file: rel(file), message: fs.existsSync(file) ? `not valid JSON: ${error.message}` : 'missing' });
    return null;
  }
};

export const placeholders = (text) => [...String(text).matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort();
const forms = (value) => (typeof value === 'string' ? { other: value } : value);
const allText = (value) => Object.values(forms(value)).join(' ');
const samePlaceholders = (a, b) => JSON.stringify([...new Set(a)].sort()) === JSON.stringify([...new Set(b)].sort());

/** Problems in one language file against English. */
export function checkLanguage(code, en, messages, file) {
  const problems = [];
  const add = (message) => problems.push({ file, message });
  const required = new Intl.PluralRules(code).resolvedOptions().pluralCategories;
  for (const key of Object.keys(en)) {
    if (!(key in messages)) {
      add(`${code}: missing key ${key}`);
      continue;
    }
    const english = en[key];
    const value = messages[key];
    if (typeof english === 'string') {
      if (typeof value !== 'string') add(`${code}: ${key} must be a string, like English`);
      else if (!value.trim()) add(`${code}: ${key} is empty`);
      else if (!samePlaceholders(placeholders(english), placeholders(value)))
        add(`${code}: ${key} placeholders {${placeholders(value).join('}, {')}} differ from English {${placeholders(english).join('}, {')}}`);
      continue;
    }
    if (typeof value !== 'object' || value === null) {
      add(`${code}: ${key} must have plural forms, like English`);
      continue;
    }
    for (const category of required) if (!value[category]?.trim()) add(`${code}: ${key} lacks the "${category}" plural form`);
    const allowed = new Set([...required, 'zero']);
    for (const category of Object.keys(value)) if (!allowed.has(category)) add(`${code}: ${key} has a plural form "${category}" ${code} doesn't use`);
    const want = placeholders(allText(english).replace(/\{count\}/g, ''));
    for (const [category, text] of Object.entries(value)) {
      const got = placeholders(String(text).replace(/\{count\}/g, ''));
      if (!samePlaceholders(want, got)) add(`${code}: ${key} (${category}) placeholders differ from English`);
    }
  }
  for (const key of Object.keys(messages)) if (!(key in en)) add(`${code}: extra key ${key} (not in English)`);
  return problems;
}

/** Review flags (FR-022): not failures, a worklist for the review pass. */
export function reviewLanguage(code, en, messages, context, glossary) {
  const flags = [];
  const keep = Array.isArray(glossary?.keep) ? glossary.keep : SHARED_KEEP;
  for (const [key, english] of Object.entries(en)) {
    const value = messages[key];
    if (value === undefined) continue;
    const enText = allText(english);
    const text = allText(value);
    const words = enText.replace(/\{\w+\}/g, '').match(/[A-Za-z]{4,}/g) ?? [];
    const isKept = (w) => keep.some((k) => k.toLowerCase().includes(w.toLowerCase()));
    if (code !== 'en' && text === enText && words.some((w) => !isKept(w))) flags.push(`${code}: ${key} looks untranslated: "${text}"`);
    const limit = /max (\d+)/.exec(context[key] ?? '')?.[1];
    if (limit && text.length > Number(limit)) flags.push(`${code}: ${key} is ${text.length} chars, over the limit ${limit}: "${text}"`);
    for (const [term, translation] of Object.entries(glossary?.terms ?? {})) {
      if (new RegExp(`\\b${term}\\b`, 'i').test(enText) && translation && !text.toLowerCase().includes(String(translation).toLowerCase().split('|')[0]))
        flags.push(`${code}: ${key} doesn't use the glossary term "${term}" -> "${translation}"`);
    }
  }
  return flags;
}

export function checkAll({ review = false } = {}) {
  const problems = [];
  const en = readJson(path.join(I18N, 'en.json'), problems);
  const context = readJson(path.join(I18N, 'en.context.json'), problems);
  if (!en) return { problems, flags: [] };
  if (context) {
    for (const key of Object.keys(en)) if (!context[key]?.trim()) problems.push({ file: 'web/src/i18n/en.context.json', message: `no context note for ${key}` });
    for (const key of Object.keys(context)) if (!(key in en)) problems.push({ file: 'web/src/i18n/en.context.json', message: `context for unknown key ${key}` });
  }
  const flags = [];
  for (const code of languageCodes()) {
    const file = path.join(LOCALES, `${code}.json`);
    const messages = readJson(file, problems);
    const glossary = readJson(path.join(GLOSSARY, `${code}.json`), problems);
    if (!messages) continue;
    problems.push(...checkLanguage(code, en, messages, rel(file)));
    if (review) flags.push(...reviewLanguage(code, en, messages, context ?? {}, glossary));
  }
  return { problems, flags };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const { problems, flags } = checkAll({ review: args.includes('--review') });
  if (args.includes('--json')) {
    process.stdout.write(JSON.stringify(problems) + '\n');
    process.exit(0);
  }
  for (const p of problems) console.log(`${p.file}: ${p.message}`);
  for (const f of flags) console.log(`review: ${f}`);
  console.log(problems.length ? `${problems.length} problem(s)` : `OK: ${languageCodes().length} languages complete`);
  process.exit(problems.length ? 1 : 0);
}
