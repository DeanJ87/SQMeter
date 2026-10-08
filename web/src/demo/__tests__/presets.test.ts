import { describe, expect, it } from 'vitest';
import { sunPosition } from '../../lib/astro';
import { LOCATION_PRESETS, resolveTimePreset } from '../presets';

const place = (id: string) => LOCATION_PRESETS.find((p) => p.id === id)!;
const LONDON = place('london');
const POLE = place('north-pole');
const SYDNEY = place('sydney');
const OCT_8_EVENING = Date.parse('2026-10-08T21:00:00Z');

const sun = (at: number, p = LONDON) => sunPosition(new Date(at), p.latitude, p.longitude).altitude;

describe('time presets', () => {
  it('dawn in London is before sunrise with the sun 6-12° down and rising (SC-006)', () => {
    const result = resolveTimePreset('dawn', OCT_8_EVENING, LONDON, LONDON.timezone);
    if (!result.ok) throw new Error(result.reason);
    expect(sun(result.at)).toBeLessThan(-6);
    expect(sun(result.at)).toBeGreaterThan(-12);
    expect(sun(result.at + 10 * 60_000)).toBeGreaterThan(sun(result.at));
  });

  it('explains there is no sunrise at the North Pole on 31 December', () => {
    const newYearsEve = resolveTimePreset('newYearsEve', OCT_8_EVENING, POLE, POLE.timezone);
    if (!newYearsEve.ok) throw new Error('31 Dec always exists');
    expect(new Date(newYearsEve.at).toISOString()).toBe('2026-12-31T23:00:00.000Z');
    const dawn = resolveTimePreset('dawn', newYearsEve.at, POLE, POLE.timezone);
    expect(dawn.ok).toBe(false);
    expect(!dawn.ok && dawn.reason).toContain('no sunrise');
  });

  it('darkest tonight is dark, and is refused under the midnight sun', () => {
    const darkest = resolveTimePreset('darkest', OCT_8_EVENING, LONDON, LONDON.timezone);
    expect(darkest.ok && sun(darkest.at)).toBeLessThan(-30);
    const midsummer = Date.parse('2026-06-21T00:00:00Z');
    expect(resolveTimePreset('darkest', midsummer, place('arctic'), 'UTC0').ok).toBe(false);
  });

  it('midsummer is in December in the southern hemisphere, in local time', () => {
    const result = resolveTimePreset('midsummer', OCT_8_EVENING, SYDNEY, SYDNEY.timezone);
    expect(result.ok && new Date(result.at).toISOString()).toBe('2026-12-20T13:00:00.000Z'); // 00:00 AEDT
    const london = resolveTimePreset('midsummer', OCT_8_EVENING, LONDON, LONDON.timezone);
    expect(london.ok && new Date(london.at).toISOString()).toBe('2026-06-20T23:00:00.000Z'); // 00:00 BST
  });

  it('now is the real time', () => {
    expect(resolveTimePreset('now', 0, LONDON, LONDON.timezone, 1234)).toEqual({ ok: true, at: 1234 });
  });
});
