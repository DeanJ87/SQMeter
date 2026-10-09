import type { SystemStatus } from '../../types';
import { darkness, formatClock, formatDuration } from '../../lib/astro';

// The darkness note under "Only send safety alerts when it's dark"
// (spec 005 FR-005). Dark-or-not is the device's own decision
// (`sky.isNight`, against the SAVED limit); only the start and end times are
// predicted here, in this browser's time zone.

type Sky = SystemStatus['sky'];

export interface DarknessInput {
  sky: Sky | undefined;
  location: { latitude: number; longitude: number } | null;
  formLimitDeg: number; // the limit in the form, possibly unsaved
  deviceNow?: Date;
}

const browserZone = () => {
  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  return zone ? ` (this browser's time, ${zone})` : " (this browser's time)";
};

export const describeDarkness = ({ sky, location, formLimitDeg, deviceNow }: DarknessInput): string | null => {
  if (!sky) return null;
  if (!sky.nightKnown || sky.sunAltitudeDeg === undefined || sky.isNight === undefined) {
    return 'Dark or not: unknown - the device needs the time and a location (Settings → Time & Location).';
  }
  const sun = sky.sunAltitudeDeg;
  const sunNow = `Sun at ${sun.toFixed(1)}° now (device)`;
  // The device compares against its saved limit; a different answer from
  // the form's limit means that limit isn't saved yet.
  const unsaved = sun < formLimitDeg !== sky.isNight ? ' Save to apply the new limit.' : '';
  if (!location) return `${sunNow} - ${sky.isNight ? 'dark' : 'not dark'}.${unsaved}`;

  const now = deviceNow ?? new Date();
  const predicted = darkness(location.latitude, location.longitude, formLimitDeg, now);
  if (sky.isNight) {
    return `${sunNow} - dark${predicted.end ? ` until ${formatClock(predicted.end)}${browserZone()}` : ''}.${unsaved}`;
  }
  const start = predicted.darkNow ? null : predicted.start;
  if (!start) return `${sunNow} - not dark yet.${unsaved}`;
  const window = `${formatClock(start)} to ${formatClock(predicted.end)}`;
  return `${sunNow} - dark in ${formatDuration(start.valueOf() - now.valueOf())}, ${window}${browserZone()}.${unsaved}`;
};
