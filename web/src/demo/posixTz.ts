// The device keeps its time zone as a POSIX TZ string (Settings → Time &
// Location), e.g. "GMT0BST,M3.5.0/1,M10.5.0". The demo works out local time
// from it the way the ESP32's C library does, so the demo device's local
// time is its own, not the browser's. Unreadable strings mean UTC, as on the
// device.

interface Rule {
  month: number; // 1-12
  week: number; // 1-5 (5 = last)
  day: number; // 0 = Sunday
  seconds: number; // local time of the change, may be negative or past 24 h
}

export interface PosixTz {
  std: string;
  stdOffset: number; // seconds east of UTC
  dst?: { name: string; offset: number; start: Rule; end: Rule };
}

const UTC: PosixTz = { std: 'UTC', stdOffset: 0 };

// POSIX offsets count west of UTC; ours count east (no -0 for UTC).
const east = (west: number) => (west === 0 ? 0 : -west);

// "1", "-5", "+5:30", "24", "-1:00:00" -> seconds
const parseTime = (text: string): number | null => {
  const match = text.match(/^([+-]?)(\d{1,3})(?::(\d{1,2}))?(?::(\d{1,2}))?$/);
  if (!match) return null;
  const sign = match[1] === '-' ? -1 : 1;
  return sign * (Number(match[2]) * 3600 + Number(match[3] ?? 0) * 60 + Number(match[4] ?? 0));
};

const parseRule = (text: string): Rule | null => {
  const match = text.match(/^M(\d{1,2})\.(\d)\.(\d)(?:\/(.+))?$/);
  if (!match) return null;
  const seconds = match[4] === undefined ? 7200 : parseTime(match[4]);
  if (seconds === null) return null;
  const month = Number(match[1]);
  const week = Number(match[2]);
  const day = Number(match[3]);
  if (month < 1 || month > 12 || week < 1 || week > 5 || day > 6) return null;
  return { month, week, day, seconds };
};

export function parsePosixTz(tz: string): PosixTz {
  const text = (tz ?? '').trim();
  // name, offset, optional dst name, optional dst offset, optional rules
  const re =
    /^(<[^>]+>|[A-Za-z]{3,})([+-]?\d{1,2}(?::\d{1,2}){0,2})(?:(<[^>]+>|[A-Za-z]{3,})([+-]?\d{1,2}(?::\d{1,2}){0,2})?(?:,([^,]+),([^,]+))?)?$/;
  const match = text.match(re);
  if (!match) return UTC;
  const name = (n: string) => n.replace(/^<|>$/g, '');
  const stdWest = parseTime(match[2]);
  if (stdWest === null) return UTC;
  const result: PosixTz = { std: name(match[1]), stdOffset: east(stdWest) };
  if (!match[3]) return result;
  const dstWest = match[4] !== undefined ? parseTime(match[4]) : stdWest - 3600;
  // Without rules POSIX assumes the US rules.
  const start = parseRule(match[5] ?? 'M3.2.0');
  const end = parseRule(match[6] ?? 'M11.1.0');
  if (dstWest === null || !start || !end) return result;
  result.dst = { name: name(match[3]), offset: east(dstWest), start, end };
  return result;
}

// The rule's day in `year`, as a UTC midnight timestamp.
const ruleDay = (year: number, rule: Rule) => {
  const first = new Date(Date.UTC(year, rule.month - 1, 1));
  let date = 1 + ((rule.day - first.getUTCDay() + 7) % 7) + (rule.week - 1) * 7;
  const days = new Date(Date.UTC(year, rule.month, 0)).getUTCDate();
  while (date > days) date -= 7;
  return Date.UTC(year, rule.month - 1, date);
};

/** Seconds east of UTC in force at `ms`. */
export function offsetAt(zone: PosixTz | string, ms: number): number {
  const tz = typeof zone === 'string' ? parsePosixTz(zone) : zone;
  if (!tz.dst) return tz.stdOffset;
  const { start, end, offset } = tz.dst;
  const year = new Date(ms + tz.stdOffset * 1000).getUTCFullYear();
  // The change to summer time happens at local standard time, back at local summer time.
  const startsAt = ruleDay(year, start) + (start.seconds - tz.stdOffset) * 1000;
  const endsAt = ruleDay(year, end) + (end.seconds - offset) * 1000;
  const summer = startsAt < endsAt ? ms >= startsAt && ms < endsAt : ms >= startsAt || ms < endsAt;
  return summer ? offset : tz.stdOffset;
}

export function zoneName(zone: PosixTz | string, ms: number): string {
  const tz = typeof zone === 'string' ? parsePosixTz(zone) : zone;
  return tz.dst && offsetAt(tz, ms) === tz.dst.offset && tz.dst.offset !== tz.stdOffset ? tz.dst.name : tz.std;
}

export interface LocalParts {
  year: number;
  month: number; // 1-12
  day: number;
  hours: number;
  minutes: number;
  seconds: number;
  offset: number; // seconds east of UTC
}

export function toLocalParts(zone: PosixTz | string, ms: number): LocalParts {
  const offset = offsetAt(zone, ms);
  const local = new Date(ms + offset * 1000);
  return {
    year: local.getUTCFullYear(),
    month: local.getUTCMonth() + 1,
    day: local.getUTCDate(),
    hours: local.getUTCHours(),
    minutes: local.getUTCMinutes(),
    seconds: local.getUTCSeconds(),
    offset,
  };
}

export interface WallTime {
  year: number;
  month: number; // 1-12
  day: number;
  hours?: number;
  minutes?: number;
}

/** The instant a local wall time (in the zone) happens. */
export function fromLocal(zone: PosixTz | string, { year, month, day, hours = 0, minutes = 0 }: WallTime): number {
  const wall = Date.UTC(year, month - 1, day, hours, minutes);
  let guess = wall - offsetAt(zone, wall) * 1000;
  guess = wall - offsetAt(zone, guess) * 1000;
  return guess;
}

const pad = (n: number, width = 2) => String(Math.abs(n)).padStart(width, '0');

/** "2026-10-08T23:10:00+0100", as the device writes `/api/status` time.iso. */
export function formatIsoWithOffset(zone: PosixTz | string, ms: number): string {
  const p = toLocalParts(zone, ms);
  const sign = p.offset < 0 ? '-' : '+';
  const off = Math.abs(p.offset);
  return `${p.year}-${pad(p.month)}-${pad(p.day)}T${pad(p.hours)}:${pad(p.minutes)}:${pad(p.seconds)}${sign}${pad(Math.floor(off / 3600))}${pad(Math.floor((off % 3600) / 60))}`;
}

/** "HH:MM" and "YYYY-MM-DD" in the zone, for the device core's clock. */
export function localClock(zone: PosixTz | string, ms: number) {
  const p = toLocalParts(zone, ms);
  return { time: `${pad(p.hours)}:${pad(p.minutes)}`, date: `${p.year}-${pad(p.month)}-${pad(p.day)}` };
}

/** "UTC+1", "UTC-4", "UTC+5:30" */
export function formatOffset(seconds: number): string {
  const sign = seconds < 0 ? '-' : '+';
  const abs = Math.abs(seconds);
  const h = Math.floor(abs / 3600);
  const m = Math.floor((abs % 3600) / 60);
  return `UTC${sign}${h}${m ? `:${pad(m)}` : ''}`;
}
