import { afterEach, describe, expect, it } from 'vitest';
import { setLanguage, type Messages } from '..';
import type { LanguageCode } from '../languages';
import { decimalSeparator, formatBytes, formatCoordinates, formatCount, formatInputNumber, formatNumber, formatTime } from '../format';
import { parseCoordinates, parseNumber } from '../parse';

// Spec 023 FR-017: numbers and dates follow the active language, with Latin
// digits; typed numbers accept the language's decimal separator and are never
// truncated.

const use = (code: LanguageCode) => setLanguage(code, code === 'en' ? null : ({} as Messages));
const value = (raw: string, options = {}) => {
  const parsed = parseNumber(raw, options);
  return parsed.ok ? parsed.value : parsed.problem;
};

afterEach(() => use('en'));

describe('formatting', () => {
  it.each([
    ['en', '21.48'],
    ['de', '21,48'],
    ['fr', '21,48'],
    ['pl', '21,48'],
    ['tr', '21,48'],
    ['ja', '21.48'],
    ['ar', '21.48'],
  ] as const)('%s writes a reading as %s', (code, expected) => {
    use(code);
    expect(formatNumber(21.48, 2)).toBe(expected);
  });

  it('groups counts the language way, never readings', () => {
    use('de');
    expect(formatCount(1234567)).toBe('1.234.567');
    expect(formatNumber(1234.5, 1)).toBe('1234,5');
    use('fr');
    expect(formatCount(1234567).replace(/\s/g, ' ')).toBe('1 234 567');
    use('en');
    expect(formatCount(65535)).toBe('65,535');
  });

  it('uses Latin digits in Arabic', () => {
    use('ar');
    expect(formatNumber(-3.5, 1)).toMatch(/^‎?-3\.5$/);
    expect(formatCount(1234)).toBe('1,234');
    expect(formatTime(new Date(Date.UTC(2026, 0, 1, 1, 2)))).toMatch(/[0-9]{2}:[0-9]{2}/);
    expect(formatTime(new Date())).not.toMatch(/[٠-٩]/);
  });

  it('formats bytes, coordinates and inputs', () => {
    use('de');
    expect(formatBytes(1536)).toBe('1,50 KB');
    expect(formatBytes(512)).toBe('512 B');
    expect(formatCoordinates(51.5074, -0.1278, 4)).toBe('51,5074 N; 0,1278 W');
    expect(formatInputNumber(0.75)).toBe('0,75');
    expect(formatInputNumber(Number.NaN)).toBe('');
    use('en');
    expect(formatCoordinates(51.5074, -0.1278, 2, true)).toBe('51.51° N, 0.13° W');
  });

  it('knows each language decimal separator', () => {
    expect(decimalSeparator('de')).toBe(',');
    expect(decimalSeparator('pt-BR')).toBe(',');
    expect(decimalSeparator('ja')).toBe('.');
    expect(decimalSeparator('zh-Hans')).toBe('.');
  });
});

describe('parseNumber', () => {
  it('reads the language decimal separator and the other as a fallback', () => {
    use('de');
    expect(value('21,5')).toBe(21.5);
    expect(value('21.5')).toBe(21.5);
    expect(value('-0,75')).toBe(-0.75);
    use('en');
    expect(value('21.5')).toBe(21.5);
    expect(value('21,5')).toBe(21.5);
  });

  it('accepts grouping only in whole groups of three', () => {
    use('de');
    expect(value('1.234,5')).toBe(1234.5);
    expect(value('60.000.000')).toBe(60000000);
    expect(value('1.23,5')).toBe('invalid');
    use('fr');
    expect(value('1 234,5')).toBe(1234.5);
    expect(value('1 234,5')).toBe(1234.5);
    expect(value('21 5')).toBe('invalid');
    use('en');
    expect(value('1,234.5')).toBe(1234.5);
  });

  it('rejects a lone fallback separator before three digits as ambiguous', () => {
    use('de');
    expect(value('1.234')).toBe('ambiguous');
    expect(value('0.750')).toBe(0.75);
    use('en');
    expect(value('1,234')).toBe('ambiguous');
  });

  it('never truncates garbage', () => {
    use('de');
    expect(value('21,5x')).toBe('invalid');
    expect(value('abc')).toBe('invalid');
    expect(value('')).toBe('empty');
    expect(value('1,5,5')).toBe('invalid');
    expect(value('2,5', { integer: true })).toBe('notInteger');
    expect(value('12', { integer: true })).toBe(12);
  });

  it('reads minus signs, Arabic-Indic digits and Arabic separators', () => {
    use('ar');
    expect(value('−' + '3.5')).toBe(-3.5);
    expect(value('٢١٫٥')).toBe(21.5);
    expect(value('١٬٢٣٤')).toBe(1234);
    expect(value('‎-2')).toBe(-2);
    use('pl');
    expect(value('–' + '4,25')).toBe(-4.25);
  });
});

describe('parseCoordinates', () => {
  it('reads pairs in every style', () => {
    expect(parseCoordinates('51.4779, -0.0015')).toEqual([51.4779, -0.0015]);
    expect(parseCoordinates('51,5074 -0,1278')).toEqual([51.5074, -0.1278]);
    expect(parseCoordinates('51,5074; -0,1278')).toEqual([51.5074, -0.1278]);
    expect(parseCoordinates('51,5074, -0,1278')).toEqual([51.5074, -0.1278]);
    expect(parseCoordinates('51.5074,-0.1278')).toEqual([51.5074, -0.1278]);
    expect(parseCoordinates('51.507 -0.127', 'de')).toEqual([51.507, -0.127]);
  });

  it('rejects anything else', () => {
    expect(parseCoordinates('London')).toBeNull();
    expect(parseCoordinates('91, 0')).toBeNull();
    expect(parseCoordinates('51.5')).toBeNull();
    expect(parseCoordinates('51,5,0,1')).toBeNull();
  });
});
