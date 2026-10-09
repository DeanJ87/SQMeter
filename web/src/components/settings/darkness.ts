import type { SystemStatus } from '../../types';
import { t } from '../../i18n';
import { formatNumber } from '../../i18n/format';
import { darkness, formatClock, formatDuration } from '../../lib/astro';

// Darkness under "Safety alerts only after dark" (spec 005 FR-005,
// spec 026 A10): two rows - the sun now and the dark period - with whose time
// it is said once, in the hint. Dark-or-not is the device's own decision
// (`sky.isNight`, against the SAVED limit); only the start and end times are
// predicted here, in this browser's time zone.

type Sky = SystemStatus['sky'];

export interface DarknessInput {
  sky: Sky | undefined;
  location: { latitude: number; longitude: number } | null;
  formLimitDeg: number; // the limit in the form, possibly unsaved
  deviceNow?: Date;
}

export interface DarknessView {
  sun?: string; // the device's sun altitude, e.g. "-15.0°"
  dark: string; // e.g. "Dark until 06:03", "19:33 to 06:03, in 2h 1m"
  hint: string; // whose time it is, or why it's unknown
  unsaved: boolean; // the form's limit would change the answer: save first
}

const zoneHint = () => {
  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  return zone ? t('settings.alerts.darknessHint', { zone }) : t('settings.alerts.darknessHintNoZone');
};

const predicted = (sky: Sky & { isNight: boolean }, location: { latitude: number; longitude: number }, limit: number, now: Date) => {
  const period = darkness(location.latitude, location.longitude, limit, now);
  if (sky.isNight)
    return period.end ? t('settings.alerts.darkUntilValue', { clock: formatClock(period.end) }) : t('settings.alerts.darkNowValue');
  const start = period.darkNow ? null : period.start;
  if (!start) return t('settings.alerts.notDarkYetValue');
  return t('settings.alerts.darkFromToValue', {
    from: formatClock(start),
    to: formatClock(period.end),
    duration: formatDuration(start.valueOf() - now.valueOf()),
  });
};

export const describeDarkness = ({ sky, location, formLimitDeg, deviceNow }: DarknessInput): DarknessView | null => {
  if (!sky) return null;
  if (!sky.nightKnown || sky.sunAltitudeDeg === undefined || sky.isNight === undefined)
    return { dark: t('dashboard.unknown'), hint: t('settings.alerts.darknessUnknownHint'), unsaved: false };
  const sun = sky.sunAltitudeDeg;
  // The device compares against its saved limit; a different answer from
  // the form's limit means that limit isn't saved yet.
  const unsaved = sun < formLimitDeg !== sky.isNight;
  const sunText = `${formatNumber(sun, 1)}°`;
  if (!location)
    return {
      sun: sunText,
      dark: t(sky.isNight ? 'settings.alerts.darkNowValue' : 'settings.alerts.notDarkValue'),
      hint: zoneHint(),
      unsaved,
    };
  const dark = predicted({ ...sky, isNight: sky.isNight }, location, formLimitDeg, deviceNow ?? new Date());
  return { sun: sunText, dark, hint: zoneHint(), unsaved };
};
