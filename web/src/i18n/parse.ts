import { decimalSeparator } from './format';

// Numbers typed by a person, in the active language (spec 023 FR-017).
//
// Rules:
// - The language's decimal separator is accepted ("21,5" in German), and so
//   is the other one as a fallback ("21.5" in German, "21,5" in English).
// - Grouping is accepted only in whole groups of three: "1.234,5" (de),
//   "1,234.5" (en), "1 234,5" (fr, any space). Anything else is rejected,
//   never truncated: parseFloat("21,5") would quietly give 21.
// - A lone fallback separator followed by exactly three digits ("1.234" in
//   German) could be grouping or decimals, so it's rejected as ambiguous
//   unless the whole part is 0 ("0.750").
// - Minus may be "-", U+2212 or an en dash; Arabic-Indic and Persian digits
//   and the Arabic decimal and thousands marks are read too.

export type NumberProblem = 'empty' | 'invalid' | 'ambiguous' | 'notInteger';
export type ParsedNumber = { ok: true; value: number } | { ok: false; problem: NumberProblem };

interface ParseOptions {
  integer?: boolean;
  /** Treat any single "." or "," as decimals, never grouping (coordinates). */
  noGrouping?: boolean;
  language?: string;
}

const DIGIT_BLOCKS = [0x0660, 0x06f0]; // Arabic-Indic, Extended Arabic-Indic (Persian/Urdu)

const normalise = (raw: string, decimal: string) =>
  [...raw]
    .map((char) => {
      const code = char.codePointAt(0) ?? 0;
      const block = DIGIT_BLOCKS.find((start) => code >= start && code <= start + 9);
      if (block !== undefined) return String(code - block);
      if (char === '\u066b') return decimal; // Arabic decimal separator
      if (char === '\u066c' || char === "'" || char === '\u2019') return ' '; // Arabic thousands, Swiss apostrophe
      if (char === '\u2212' || char === '\u2013') return '-'; // minus sign, en dash
      if (/[\u200e\u200f\u061c]/.test(char)) return ''; // direction marks
      if (/\s/.test(char)) return ' '; // incl. no-break and narrow no-break spaces (fr, pl)
      return char;
    })
    .join('')
    .trim();

const fail = (problem: NumberProblem): ParsedNumber => ({ ok: false, problem });

// Strips grouping from a whole-number part; null if the groups aren't valid.
const ungroup = (whole: string): string | null => {
  if (/^\d*$/.test(whole)) return whole;
  const match = whole.match(/^\d{1,3}([ .,])\d{3}(?:\1\d{3})*$/);
  return match ? whole.split(match[1]).join('') : null;
};

// Which character marks the decimals in `body`, '' if none; null if it can't tell.
const findDecimal = (body: string, decimal: string, noGrouping: boolean): string | null => {
  const fallback = decimal === '.' ? ',' : '.';
  if (body.includes(decimal)) return decimal;
  const count = body.split(fallback).length - 1;
  if (count === 0) return '';
  if (noGrouping) return count === 1 ? fallback : null;
  if (count > 1) return ''; // only grouping can repeat
  const [whole, fraction] = body.split(fallback);
  const ambiguous = fraction.length === 3 && /^\d+$/.test(whole.replace(/ /g, '')) && Number(whole.replace(/ /g, '')) !== 0;
  return ambiguous ? null : fallback;
};

// The whole and fractional digits of `body`, grouping removed; null if malformed.
const digitsOf = (body: string, separator: string, noGrouping: boolean): { whole: string; fraction: string } | null => {
  const parts = separator ? body.split(separator) : [body];
  if (parts.length > 2) return null;
  const whole = noGrouping ? parts[0] : ungroup(parts[0]);
  const fraction = parts[1] ?? '';
  const valid = whole !== null && /^\d*$/.test(whole) && /^\d*$/.test(fraction) && (whole !== '' || fraction !== '');
  return valid ? { whole: whole as string, fraction } : null;
};

export function parseNumber(raw: string, options: ParseOptions = {}): ParsedNumber {
  const decimal = decimalSeparator(options.language);
  const text = normalise(raw, decimal);
  if (text === '') return fail('empty');
  const match = text.match(/^([+-]?)\s*([\d., ]+)$/);
  if (!match) return fail('invalid');
  const [, sign, body] = match;
  const separator = findDecimal(body, decimal, options.noGrouping ?? false);
  if (separator === null) return fail('ambiguous');
  const digits = digitsOf(body, separator, options.noGrouping ?? false);
  if (!digits) return fail('invalid');
  const value = Number(`${sign}${digits.whole || '0'}.${digits.fraction || '0'}`);
  if (!Number.isFinite(value)) return fail('invalid');
  if (options.integer && !Number.isInteger(value)) return fail('notInteger');
  return { ok: true, value };
}

/** "lat, lon" as typed or pasted, in any language's style; null if it isn't a valid pair. */
export function parseCoordinates(text: string, language?: string): [number, number] | null {
  const trimmed = text.trim();
  let parts: string[];
  if (trimmed.includes(';')) parts = trimmed.split(';');
  else if (/,\s/.test(trimmed)) parts = trimmed.split(/,\s+/);
  else if ((trimmed.match(/,/g) ?? []).length === 1 && !/\s/.test(trimmed)) parts = trimmed.split(',');
  else parts = trimmed.split(/\s+/);
  if (parts.length !== 2) return null;
  const [lat, lon] = parts.map((part) => parseNumber(part, { noGrouping: true, language }));
  if (!lat.ok || !lon.ok) return null;
  return Math.abs(lat.value) <= 90 && Math.abs(lon.value) <= 180 ? [lat.value, lon.value] : null;
}
