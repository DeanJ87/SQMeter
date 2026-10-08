// Low-precision sun and moon positions (after the formulas in Meeus /
// SunCalc). Good to about a minute for sun times and a few minutes for the
// moon - plenty for planning a night, and it costs the device nothing.

const RAD = Math.PI / 180;
const DAY_MS = 86400000;
const J2000 = 2451545;
const OBLIQUITY = RAD * 23.4397;

const toDays = (date: Date) => date.valueOf() / DAY_MS + 2440587.5 - J2000;

const rightAscension = (l: number, b: number) =>
  Math.atan2(Math.sin(l) * Math.cos(OBLIQUITY) - Math.tan(b) * Math.sin(OBLIQUITY), Math.cos(l));
const declination = (l: number, b: number) =>
  Math.asin(Math.sin(b) * Math.cos(OBLIQUITY) + Math.cos(b) * Math.sin(OBLIQUITY) * Math.sin(l));
const siderealTime = (d: number, lw: number) => RAD * (280.16 + 360.9856235 * d) - lw;

const sunCoords = (d: number) => {
  const m = RAD * (357.5291 + 0.98560028 * d);
  const c = RAD * (1.9148 * Math.sin(m) + 0.02 * Math.sin(2 * m) + 0.0003 * Math.sin(3 * m));
  const l = m + c + RAD * 102.9372 + Math.PI;
  return { dec: declination(l, 0), ra: rightAscension(l, 0) };
};

const moonCoords = (d: number) => {
  const l = RAD * (218.316 + 13.176396 * d);
  const m = RAD * (134.963 + 13.064993 * d);
  const f = RAD * (93.272 + 13.22935 * d);
  const lon = l + RAD * 6.289 * Math.sin(m);
  const lat = RAD * 5.128 * Math.sin(f);
  return { ra: rightAscension(lon, lat), dec: declination(lon, lat), distKm: 385001 - 20905 * Math.cos(m) };
};

const horizontal = (ra: number, dec: number, d: number, lat: number, lon: number) => {
  const phi = RAD * lat;
  const h = siderealTime(d, RAD * -lon) - ra;
  const altitude = Math.asin(Math.sin(phi) * Math.sin(dec) + Math.cos(phi) * Math.cos(dec) * Math.cos(h));
  const azimuth = Math.atan2(Math.sin(h), Math.cos(h) * Math.sin(phi) - Math.tan(dec) * Math.cos(phi));
  // Degrees; azimuth from north, clockwise.
  return { altitude: altitude / RAD, azimuth: (((azimuth / RAD + 180) % 360) + 360) % 360 };
};

export const sunPosition = (date: Date, lat: number, lon: number) => {
  const d = toDays(date);
  const { ra, dec } = sunCoords(d);
  return horizontal(ra, dec, d, lat, lon);
};

export const moonPosition = (date: Date, lat: number, lon: number) => {
  const d = toDays(date);
  const { ra, dec } = moonCoords(d);
  return horizontal(ra, dec, d, lat, lon);
};

// fraction: 0 new .. 1 full. phase: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter.
export const moonIllumination = (date: Date) => {
  const d = toDays(date);
  const s = sunCoords(d);
  const m = moonCoords(d);
  const sunDistKm = 149598000;
  const phi = Math.acos(Math.sin(s.dec) * Math.sin(m.dec) + Math.cos(s.dec) * Math.cos(m.dec) * Math.cos(s.ra - m.ra));
  const inc = Math.atan2(sunDistKm * Math.sin(phi), m.distKm - sunDistKm * Math.cos(phi));
  const angle = Math.atan2(
    Math.cos(s.dec) * Math.sin(s.ra - m.ra),
    Math.sin(s.dec) * Math.cos(m.dec) - Math.cos(s.dec) * Math.sin(m.dec) * Math.cos(s.ra - m.ra),
  );
  return { fraction: (1 + Math.cos(inc)) / 2, phase: 0.5 + (0.5 * inc * (angle < 0 ? -1 : 1)) / Math.PI };
};

export const moonPhaseName = (phase: number) => {
  const names = [
    'New moon',
    'Waxing crescent',
    'First quarter',
    'Waxing gibbous',
    'Full moon',
    'Waning gibbous',
    'Last quarter',
    'Waning crescent',
  ];
  return names[Math.round(phase * 8) % 8];
};

// Standard altitudes, degrees.
export const SUN_HORIZON = -0.833;
export const MOON_HORIZON = 0.125;

export interface Crossing {
  time: Date;
  rising: boolean;
}

// Times in [from, from + hours] when `altitudeAt` crosses `threshold`.
// Steps every 10 minutes and refines by bisection to the second.
export const crossings = (altitudeAt: (date: Date) => number, threshold: number, from: Date, hours = 36): Crossing[] => {
  const step = 10 * 60000;
  const end = from.valueOf() + hours * 3600000;
  const result: Crossing[] = [];
  let t0 = from.valueOf();
  let a0 = altitudeAt(from) - threshold;
  while (t0 < end) {
    const t1 = Math.min(t0 + step, end);
    const a1 = altitudeAt(new Date(t1)) - threshold;
    if (a0 < 0 !== a1 < 0) {
      let lo = t0;
      let hi = t1;
      while (hi - lo > 1000) {
        const mid = (lo + hi) / 2;
        if (altitudeAt(new Date(mid)) - threshold < 0 === a0 < 0) lo = mid;
        else hi = mid;
      }
      result.push({ time: new Date(Math.round((lo + hi) / 2)), rising: a1 > a0 });
    }
    t0 = t1;
    a0 = a1;
  }
  return result;
};

export const nextCrossing = (altitudeAt: (date: Date) => number, threshold: number, from: Date, rising: boolean, hours = 36) =>
  crossings(altitudeAt, threshold, from, hours).find((c) => c.rising === rising)?.time ?? null;

export type SkyPhase = 'day' | 'civil' | 'nautical' | 'astronomical' | 'night';

export const skyPhase = (sunAltitude: number): SkyPhase =>
  sunAltitude > SUN_HORIZON
    ? 'day'
    : sunAltitude > -6
      ? 'civil'
      : sunAltitude > -12
        ? 'nautical'
        : sunAltitude > -18
          ? 'astronomical'
          : 'night';

export const SKY_PHASE_LABEL: Record<SkyPhase, string> = {
  day: 'Daylight',
  civil: 'Civil twilight',
  nautical: 'Nautical twilight',
  astronomical: 'Astronomical twilight',
  night: 'Dark',
};

// When the sun next passes below / above `darkAltitude`. If it's dark now,
// `start` is null and `end` is when it gets light; otherwise `start` is when
// darkness begins and `end` when it ends after that. Null where it doesn't
// happen within 36 h (polar summer/winter).
export const darkness = (lat: number, lon: number, darkAltitude: number, now: Date) => {
  const altitudeAt = (date: Date) => sunPosition(date, lat, lon).altitude;
  const darkNow = altitudeAt(now) < darkAltitude;
  const start = darkNow ? null : nextCrossing(altitudeAt, darkAltitude, now, false);
  const end = nextCrossing(altitudeAt, darkAltitude, start ?? now, true);
  return { darkNow, start, end };
};

export const formatClock = (date: Date | null) => (date ? date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : '--');

export const formatDuration = (ms: number) => {
  const minutes = Math.max(0, Math.round(ms / 60000));
  const hours = Math.floor(minutes / 60);
  return hours > 0 ? `${hours}h ${minutes % 60}m` : `${minutes}m`;
};
