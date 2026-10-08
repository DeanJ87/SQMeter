import { nextCrossing, SUN_HORIZON, sunPosition } from '../lib/astro';
import { fromLocal, toLocalParts } from './posixTz';

// Date, time and place presets for the demo device (spec 019 US4,
// research R5/R6). Times are worked out at the device's location and in its
// time zone; a preset that can't happen there (no sunrise in polar night)
// says why instead of moving the clock.

export interface Place {
  latitude: number;
  longitude: number;
}

export type TimePresetId = 'now' | 'darkest' | 'dawn' | 'dusk' | 'midsummer' | 'midwinter' | 'newYearsEve';

export const TIME_PRESETS: { id: TimePresetId; label: string }[] = [
  { id: 'now', label: 'Now' },
  { id: 'darkest', label: 'Darkest tonight' },
  { id: 'dawn', label: 'Dawn' },
  { id: 'dusk', label: 'Dusk' },
  { id: 'midsummer', label: 'Midsummer midnight' },
  { id: 'midwinter', label: 'Midwinter midnight' },
  { id: 'newYearsEve', label: '31 Dec 23:00' },
];

export type TimeResult = { ok: true; at: number } | { ok: false; reason: string };

// Dawn: this long before sunrise (FR-013).
const DAWN_BEFORE_SUNRISE_MS = 45 * 60_000;
// Below this the sky is at least nautical twilight; a "night" that never gets there isn't one.
const DARK_ENOUGH_DEG = -6;
const DAY_MS = 24 * 3_600_000;

const sunAt = (place: Place) => (date: Date) => sunPosition(date, place.latitude, place.longitude).altitude;

// The lowest sun in the next 24 hours, at 10-minute steps.
export function darkestTime(fromMs: number, place: Place) {
  let best = fromMs;
  let lowest = Infinity;
  for (let t = fromMs; t < fromMs + DAY_MS; t += 10 * 60_000) {
    const altitude = sunAt(place)(new Date(t));
    if (altitude < lowest) {
      lowest = altitude;
      best = t;
    }
  }
  return { at: best, altitude: lowest };
}

const NO_SUNRISE = "There's no sunrise here in the next 24 hours (polar night or midnight sun), so the clock stays where it is.";
const NO_SUNSET = "There's no sunset here in the next 24 hours (polar night or midnight sun), so the clock stays where it is.";

export function resolveTimePreset(id: TimePresetId, nowMs: number, place: Place, timezone: string, wallNowMs = Date.now()): TimeResult {
  const { year } = toLocalParts(timezone, nowMs);
  const south = place.latitude < 0;
  switch (id) {
    case 'now':
      return { ok: true, at: wallNowMs };
    case 'darkest': {
      const darkest = darkestTime(nowMs, place);
      if (darkest.altitude > DARK_ENOUGH_DEG)
        return { ok: false, reason: `It doesn't get dark here in the next 24 hours: the sun stays above ${DARK_ENOUGH_DEG}°.` };
      return { ok: true, at: darkest.at };
    }
    case 'dawn': {
      const sunrise = nextCrossing(sunAt(place), SUN_HORIZON, new Date(nowMs), true, 24);
      return sunrise ? { ok: true, at: sunrise.getTime() - DAWN_BEFORE_SUNRISE_MS } : { ok: false, reason: NO_SUNRISE };
    }
    case 'dusk': {
      const sunset = nextCrossing(sunAt(place), SUN_HORIZON, new Date(nowMs), false, 24);
      return sunset ? { ok: true, at: sunset.getTime() } : { ok: false, reason: NO_SUNSET };
    }
    case 'midsummer':
      return { ok: true, at: fromLocal(timezone, { year, month: south ? 12 : 6, day: 21 }) };
    case 'midwinter':
      return { ok: true, at: fromLocal(timezone, { year, month: south ? 6 : 12, day: 21 }) };
    case 'newYearsEve':
      return { ok: true, at: fromLocal(timezone, { year, month: 12, day: 31, hours: 23 }) };
  }
}

export interface LocationPreset extends Place {
  id: string;
  label: string;
  elevation: number; // metres; the simulated GPS reports it
  timezone: string; // POSIX, the device's own format
}

// Height the GPS reports away from the presets.
export const DEFAULT_ELEVATION = 42;

export const LOCATION_PRESETS: LocationPreset[] = [
  { id: 'london', label: 'London', latitude: 51.5074, longitude: -0.1278, elevation: 35, timezone: 'GMT0BST,M3.5.0/1,M10.5.0' },
  { id: 'la-palma', label: 'La Palma', latitude: 28.7606, longitude: -17.8816, elevation: 2396, timezone: 'WET0WEST,M3.5.0/1,M10.5.0' },
  {
    id: 'atacama',
    label: 'Atacama',
    latitude: -24.6272,
    longitude: -70.4041,
    elevation: 2635,
    timezone: '<-04>4<-03>,M9.1.6/24,M4.1.6/24',
  },
  { id: 'sydney', label: 'Sydney', latitude: -33.8688, longitude: 151.2093, elevation: 58, timezone: 'AEST-10AEDT,M10.1.0,M4.1.0/3' },
  { id: 'arctic', label: '75° N 1° W', latitude: 75, longitude: -1, elevation: 0, timezone: 'UTC0' },
  { id: 'north-pole', label: 'North Pole', latitude: 90, longitude: 0, elevation: 0, timezone: 'UTC0' },
];

/** The preset at `place`, if it is one. */
export const presetAt = (place: Place) =>
  LOCATION_PRESETS.find((p) => Math.abs(p.latitude - place.latitude) < 1e-3 && Math.abs(p.longitude - place.longitude) < 1e-3);
