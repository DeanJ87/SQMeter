import { describe, it, expect } from 'vitest';
import { crossings, darkness, moonIllumination, moonPhaseName, sunPosition, SUN_HORIZON } from '../astro';

const GREENWICH = { lat: 51.4779, lon: -0.0015 };
const minutesBetween = (a: Date, b: Date) => Math.abs(a.valueOf() - b.valueOf()) / 60000;

describe('astro', () => {
  it('puts the sun near its solstice noon altitude', () => {
    // 90 - 51.48 + 23.44 = 61.96 at local noon on the June solstice
    const { altitude, azimuth } = sunPosition(new Date('2026-06-21T12:02:00Z'), GREENWICH.lat, GREENWICH.lon);
    expect(altitude).toBeGreaterThan(61.5);
    expect(altitude).toBeLessThan(62.3);
    expect(Math.abs(azimuth - 180)).toBeLessThan(2);
  });

  it('finds Greenwich sunrise and sunset on the June solstice', () => {
    const altitudeAt = (d: Date) => sunPosition(d, GREENWICH.lat, GREENWICH.lon).altitude;
    const events = crossings(altitudeAt, SUN_HORIZON, new Date('2026-06-21T00:00:00Z'), 24);
    const rise = events.find((e) => e.rising)!.time;
    const set = events.find((e) => !e.rising)!.time;
    expect(minutesBetween(rise, new Date('2026-06-21T03:43:00Z'))).toBeLessThan(3);
    expect(minutesBetween(set, new Date('2026-06-21T20:21:00Z'))).toBeLessThan(3);
  });

  it('has no astronomical dark in a London midsummer night', () => {
    const { start } = darkness(GREENWICH.lat, GREENWICH.lon, -18, new Date('2026-06-21T12:00:00Z'));
    expect(start).toBeNull();
  });

  it('reports astronomical darkness in winter', () => {
    const { darkNow, start, end } = darkness(GREENWICH.lat, GREENWICH.lon, -18, new Date('2026-12-21T12:00:00Z'));
    expect(darkNow).toBe(false);
    expect(start!.getUTCHours()).toBe(17);
    expect(end!.valueOf()).toBeGreaterThan(start!.valueOf());
  });

  it('knows full and new moons', () => {
    const full = moonIllumination(new Date('2026-03-03T11:38:00Z'));
    expect(full.fraction).toBeGreaterThan(0.99);
    expect(moonPhaseName(full.phase)).toBe('Full moon');
    const fresh = moonIllumination(new Date('2026-02-17T12:01:00Z'));
    expect(fresh.fraction).toBeLessThan(0.01);
    expect(moonPhaseName(fresh.phase)).toBe('New moon');
  });
});
