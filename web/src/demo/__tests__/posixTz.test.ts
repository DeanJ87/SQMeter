import { describe, expect, it } from 'vitest';
import { formatIsoWithOffset, fromLocal, localClock, offsetAt, parsePosixTz, zoneName } from '../posixTz';

const LONDON = 'GMT0BST,M3.5.0/1,M10.5.0';
const PARIS = 'CET-1CEST,M3.5.0,M10.5.0/3';
const SYDNEY = 'AEST-10AEDT,M10.1.0,M4.1.0/3';
const CHILE = '<-04>4<-03>,M9.1.6/24,M4.1.6/24';

const at = (iso: string) => Date.parse(iso);

describe('POSIX time zones', () => {
  it('parses names, offsets and rules', () => {
    expect(parsePosixTz('JST-9')).toEqual({ std: 'JST', stdOffset: 9 * 3600 });
    const chile = parsePosixTz(CHILE);
    expect(chile.std).toBe('-04');
    expect(chile.stdOffset).toBe(-4 * 3600);
    expect(chile.dst?.offset).toBe(-3 * 3600);
    expect(chile.dst?.start.seconds).toBe(24 * 3600);
  });

  it('falls back to UTC for strings it cannot read, like the device', () => {
    expect(offsetAt('nonsense', at('2026-07-01T00:00:00Z'))).toBe(0);
    expect(offsetAt('', at('2026-07-01T00:00:00Z'))).toBe(0);
    expect(offsetAt('UTC0', at('2026-07-01T00:00:00Z'))).toBe(0);
  });

  it('switches London at 01:00 UTC on the last Sundays of March and October', () => {
    expect(offsetAt(LONDON, at('2026-03-29T00:59:00Z'))).toBe(0);
    expect(offsetAt(LONDON, at('2026-03-29T01:00:00Z'))).toBe(3600);
    expect(offsetAt(LONDON, at('2026-10-25T00:59:00Z'))).toBe(3600);
    expect(offsetAt(LONDON, at('2026-10-25T01:00:00Z'))).toBe(0);
    expect(zoneName(LONDON, at('2026-07-01T12:00:00Z'))).toBe('BST');
    expect(zoneName(LONDON, at('2026-12-01T12:00:00Z'))).toBe('GMT');
  });

  it('handles Paris and the southern hemisphere', () => {
    expect(offsetAt(PARIS, at('2026-07-01T12:00:00Z'))).toBe(7200);
    expect(offsetAt(PARIS, at('2026-01-01T12:00:00Z'))).toBe(3600);
    // Sydney: summer time from the first Sunday of October to the first Sunday of April.
    expect(offsetAt(SYDNEY, at('2026-12-31T12:00:00Z'))).toBe(11 * 3600);
    expect(offsetAt(SYDNEY, at('2026-06-30T12:00:00Z'))).toBe(10 * 3600);
    expect(offsetAt(CHILE, at('2026-12-31T12:00:00Z'))).toBe(-3 * 3600);
    expect(offsetAt(CHILE, at('2026-06-30T12:00:00Z'))).toBe(-4 * 3600);
  });

  it('formats like the device and converts local times back', () => {
    expect(formatIsoWithOffset(LONDON, at('2026-10-08T22:10:00Z'))).toBe('2026-10-08T23:10:00+0100');
    expect(formatIsoWithOffset(CHILE, at('2026-06-30T12:00:00Z'))).toBe('2026-06-30T08:00:00-0400');
    expect(localClock(SYDNEY, at('2026-12-31T12:30:00Z'))).toEqual({ time: '23:30', date: '2026-12-31' });
    expect(new Date(fromLocal(LONDON, { year: 2026, month: 12, day: 31, hours: 23 })).toISOString()).toBe('2026-12-31T23:00:00.000Z');
    expect(new Date(fromLocal(LONDON, { year: 2026, month: 6, day: 21 })).toISOString()).toBe('2026-06-20T23:00:00.000Z');
    expect(new Date(fromLocal(SYDNEY, { year: 2026, month: 12, day: 21 })).toISOString()).toBe('2026-12-20T13:00:00.000Z');
  });
});
